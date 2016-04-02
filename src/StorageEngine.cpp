/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Pavel Ognev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "StorageEngine.h"

#include <QtCore/QFile>

#include <openssl/asn1.h>
#include <openssl/rand.h>
#include <openssl/evp.h>


#define ENC_CIPHER      EVP_aes_256_gcm
#define ENC_IV_SIZE     12
#define ENC_MAC_SIZE    16

#define KDF_HASH        EVP_sha256
#define KDF_ITERATIONS  1   // 1 is enough for good password. No options for bad passwords.

#define FILE_APPENDIX_SIZE (ENC_IV_SIZE + ENC_MAC_SIZE)

/*
 * File format:
 * +--------------------------------------------------+---------------+----------------+
 * | Encrypted DER-encoded Payload (various size > 0) | IV (12 bytes) | MAC (16 bytes) |
 * +--------------------------------------------------+---------------+----------------+
 *
 *
 * Payload format (ASN.1):
 *
 * Payload ::= SEQUENCE OF PasswordEntry
 *
 * PasswordEntry ::= SET OF DataCell
 *
 * DataCell ::= CHOICE {
 *    ServiceName   [0]  UTF8String
 *    UserLogin     [1]  UTF8String
 *    UserPassword  [2]  UTF8String
 *    CommentsText  [16] UTF8String
 *    -- other tag values are reserved for future use --
 * }
 *
 *
 * References:
 *
 * ITU-T Recommendation X.680 (2002) | ISO/IEC 8824-1:2002,
 * Information technology - Abstract Syntax Notation One
 * (ASN.1):  Specification of basic notation.
 *
 * ITU-T Recommendation X.690 (2002) | ISO/IEC 8825-1:2002,
 * Information technology - ASN.1 encoding rules:
 * Specification of Basic Encoding Rules (BER), Canonical
 * Encoding Rules (CER) and Distinguished Encoding Rules (DER).
 *
 */

static const char kdf_salt[] = "PassKeeper key generation";
static const int cellTags[DATA_COLS_COUNT] = { 0, 1, 2, 16 };


DataRow::DataRow()
{
}


DataRow::DataRow( const uint8_t **ptr, const uint8_t *endPtr )
{
    // There is an entry parsing routine here

    const uint8_t *curPtr = *ptr;
    const uint8_t *entryEnd;
    long length;
    int tag;
    int xclass;

    if( ASN1_get_object( &curPtr, &length, &tag, &xclass, endPtr - curPtr ) != V_ASN1_CONSTRUCTED ||
        V_ASN1_SET != tag || V_ASN1_UNIVERSAL != xclass || curPtr + length > endPtr )
    {
        // Global structure parsing error. Rewind to the end
        *ptr = endPtr;
        return;
    }

    entryEnd = curPtr + length;

    // Set pointer to the next entry
    *ptr = entryEnd;

    while( curPtr < entryEnd )
    {
        if( ASN1_get_object( &curPtr, &length, &tag, &xclass, entryEnd - curPtr ) != 0 ||
            V_ASN1_CONTEXT_SPECIFIC != xclass )
        {
            // Substructure parsing error. Exit
            return;
        }

        // Choose cell by tag
        for( int i = 0; i < DATA_COLS_COUNT; ++i )
            if( tag == cellTags[i] )
            {
                cells[i].append( (const char*)curPtr, length );
                break;
            }

        curPtr += length;
    }
}


size_t DataRow::encode( uint8_t *dst, size_t maxSize ) const
{
    // Calculate required memory first
    size_t entryDataSize = 0;

    for( int i = 0; i < DATA_COLS_COUNT; ++i )
        if( !cells[i].isEmpty() )
            entryDataSize += ASN1_object_size( 0, cells[i].length(), cellTags[i] );

    if( 0 == entryDataSize )
        return 0;

    const size_t entrySize = ASN1_object_size( 1, entryDataSize, V_ASN1_SET );

    if( NULL != dst )
    {
        if( entrySize > maxSize )
            return 0;

        ASN1_put_object( &dst, 1, entryDataSize, V_ASN1_SET, V_ASN1_UNIVERSAL );

        for( int i = 0; i < DATA_COLS_COUNT; ++i )
            if( !cells[i].isEmpty() )
            {
                const int cellDataLength = cells[i].length();

                ASN1_put_object( &dst, 0, cellDataLength, cellTags[i], V_ASN1_CONTEXT_SPECIFIC );

                memcpy( dst, cells[i].constData(), cellDataLength );
                dst += cellDataLength;
            }
    }

    return entrySize;
}


bool DataRow::operator < ( const DataRow &other ) const
{
    for( int i = 0; i < DATA_COLS_COUNT; ++i )
    {
        int cmpRes = strcmp( cells[i].constData(), other.cells[i].constData() );
        if( cmpRes < 0 ) return true;
        if( cmpRes > 0 ) return false;
    }

    return false;
}


StorageEngine::StorageEngine( const QString &file )
: dbFileName( file )
{
}


StorageEngine::~StorageEngine()
{
    /*
     * No explicit key material cleanup.
     * Any released memory of the application must be cleaned,
     * since a lot of sensitive data is stored in Qt containers.
     */
}


bool StorageEngine::setPassword( const QString &password )
{
    QByteArray passUtf8 = password.toUtf8();

    key.resize( EVP_CIPHER_key_length( ENC_CIPHER() ) );

    int status = PKCS5_PBKDF2_HMAC( passUtf8.constData(), passUtf8.length(),
                                    (const uint8_t*)kdf_salt, sizeof(kdf_salt) - 1,
                                    KDF_ITERATIONS, KDF_HASH(),
                                    key.length(), (uint8_t*)key.data() );
    if( 1 != status )
    {
        errorDescription = "Key derivation failure";
        return false;
    }

    return true;
}


bool StorageEngine::readDbFile()
{
    QByteArray fileContent;
    if( !readFileContent( &fileContent ) )
    {
        errorDescription = "Cannot open DB file: ";
        errorDescription += dbFileName;
        return false;
    }

    if( !decryptData( &fileContent ) )
    {
        errorDescription = "Wrong password or file corruption";
        return false;
    }

    // Parse DB structure
    const uint8_t *curPtr = (const uint8_t*)fileContent.constData();
    const uint8_t *endPtr = curPtr + fileContent.length();

    long length;
    int tag;
    int xclass;
    if( ASN1_get_object( &curPtr, &length, &tag, &xclass, endPtr - curPtr ) != V_ASN1_CONSTRUCTED ||
        V_ASN1_SEQUENCE != tag || V_ASN1_UNIVERSAL != xclass || curPtr + length != endPtr )
    {
        errorDescription = "Password DB structure is corrupted";
        return false;
    }

    data.clear();
    while( curPtr < endPtr )
    {
        DataRow curEntry( &curPtr, endPtr );
        data.insert( curEntry );
    }

    return true;
}


bool StorageEngine::writeDbFile()
{
    QByteArray fileContent;

    size_t sequenceInnerSize = 0;
    for( std::multiset<DataRow>::iterator i = data.begin(); i != data.end(); ++i )
        sequenceInnerSize += i->encode( NULL, 0 );

    const size_t dataSize = ASN1_object_size( 1, sequenceInnerSize, V_ASN1_SEQUENCE );
    fileContent.reserve( dataSize + FILE_APPENDIX_SIZE );
    fileContent.resize( dataSize );

    uint8_t *writePtr = (uint8_t*)fileContent.data();
    uint8_t *endPtr = writePtr + dataSize;

    ASN1_put_object( &writePtr, 1, sequenceInnerSize, V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL );
    for( std::multiset<DataRow>::iterator i = data.begin(); i != data.end(); ++i )
        writePtr += i->encode( writePtr, endPtr - writePtr );

    if( writePtr != endPtr )
    {
        errorDescription = "Error serializing the data";
        return false;
    }

    if( !encryptData( &fileContent ) )
    {
        errorDescription = "Error encrypting the data";
        return false;
    }

    if( !writeFileContent( fileContent ) )
    {
        // errorDescription is set inside writeFileContent() function
        return false;
    }

    return true;
}


QString StorageEngine::getError()
{
    return errorDescription;
}


bool StorageEngine::readFileContent( QByteArray *dst )
{
    QFile dbFile( dbFileName );

    if( !dbFile.open( QIODevice::ReadOnly ) )
        return false;

    *dst = dbFile.readAll();

    dbFile.close();
    return true;
}


bool StorageEngine::writeFileContent( const QByteArray &buf )
{
    QFile oldFile( dbFileName );
    QFile newFile( "" );

    int suffixIndex = 0;
    do
    {
        newFile.setFileName( dbFileName + "_" + QString::number( suffixIndex ) );
        suffixIndex++;
    }
    while( newFile.exists() );

    if( !newFile.open( QIODevice::WriteOnly ) )
    {
        errorDescription = "Cannot create indexed file";
        return false;
    }

    if( newFile.write( buf ) == -1 )
    {
        errorDescription = "Error writing to the file";
        newFile.close();
        newFile.remove();

        return false;
    }

    newFile.close();

    if( oldFile.exists() && !oldFile.remove() )
    {
        errorDescription = "Cannot remove previous version of DB file.\nNew one is saved under name \""
            + newFile.fileName() + "\"\nPlease resolve it manually or try again.";
        return false;
    }

    if( !newFile.rename( dbFileName ) )
    {
        errorDescription = "Cannot rename new DB file.\nIt is saved under name \""
            + newFile.fileName() + "\"\nPlease resolve it manually or try again.";
        return false;
    }

    return true;
}


bool StorageEngine::encryptData( QByteArray *buf )
{
    const EVP_CIPHER *cipher = ENC_CIPHER();
    if( EVP_CIPHER_iv_length ( cipher ) != ENC_IV_SIZE ||
        EVP_CIPHER_key_length( cipher ) != key.length() )
    {
        return false;
    }

    EVP_CIPHER_CTX *cipherCtx = NULL;
    bool result = false;
    int outLen;

    const size_t payloadSize = buf->length();
    buf->resize( payloadSize + FILE_APPENDIX_SIZE );

    uint8_t *payloadPtr = (uint8_t*)buf->data();
    uint8_t *ivPtr = payloadPtr + payloadSize;
    uint8_t *macPtr = ivPtr + ENC_IV_SIZE;

    if( !RAND_bytes( ivPtr, ENC_IV_SIZE ) )
        goto stop;

    cipherCtx = EVP_CIPHER_CTX_new();
    if( NULL == cipherCtx )
        goto stop;

    if( !EVP_EncryptInit( cipherCtx, cipher, (const uint8_t*)key.constData(), ivPtr ) )
        goto stop;

    if( !EVP_EncryptUpdate( cipherCtx, payloadPtr, &outLen, payloadPtr, payloadSize ) ||
        outLen != (int)payloadSize )
        goto stop;

    if( !EVP_EncryptFinal( cipherCtx, payloadPtr + payloadSize, &outLen ) || 0 != outLen )
        goto stop;

    if( !EVP_CIPHER_CTX_ctrl(cipherCtx, EVP_CTRL_GCM_GET_TAG, ENC_MAC_SIZE, macPtr ) )
        goto stop;

    result = true;

stop:
    if( NULL != cipherCtx )
        EVP_CIPHER_CTX_free( cipherCtx );

    return result;
}


bool StorageEngine::decryptData( QByteArray *buf )
{
    const EVP_CIPHER *cipher = ENC_CIPHER();
    if( EVP_CIPHER_iv_length ( cipher ) != ENC_IV_SIZE ||
        EVP_CIPHER_key_length( cipher ) != key.length() ||
        buf->length() <= FILE_APPENDIX_SIZE )
    {
        return false;
    }

    EVP_CIPHER_CTX *cipherCtx = NULL;
    bool result = false;
    int outLen;

    const size_t payloadSize = buf->length() - FILE_APPENDIX_SIZE;
    uint8_t *payloadPtr = (uint8_t*)buf->data();
    const uint8_t *ivPtr = payloadPtr + payloadSize;
    const uint8_t *macPtr = ivPtr + ENC_IV_SIZE;

    cipherCtx = EVP_CIPHER_CTX_new();
    if( NULL == cipherCtx )
        goto stop;

    if( !EVP_DecryptInit( cipherCtx, cipher, (const uint8_t*)key.constData(), ivPtr ) )
        goto stop;

    if( !EVP_DecryptUpdate( cipherCtx, payloadPtr, &outLen, payloadPtr, payloadSize ) ||
        outLen != (int)payloadSize )
        goto stop;

    if( !EVP_CIPHER_CTX_ctrl(cipherCtx, EVP_CTRL_GCM_SET_TAG, ENC_MAC_SIZE, (uint8_t*)macPtr ) )
        goto stop;

    if( !EVP_DecryptFinal( cipherCtx, payloadPtr + payloadSize, &outLen ) || 0 != outLen )
        goto stop;

    buf->resize( payloadSize );
    result = true;

stop:
    if( NULL != cipherCtx )
        EVP_CIPHER_CTX_free( cipherCtx );

    return result;
}
