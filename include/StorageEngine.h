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

#ifndef STORAGE_ENGINE_H
#define STORAGE_ENGINE_H

#include <QtCore/QString>
#include <stdint.h>
#include <set>

#define DATA_COLS_COUNT 4


class DataRow
{
public:
    DataRow();
    DataRow( const uint8_t **ptr, const uint8_t *endPtr );

    size_t encode( uint8_t *dst, size_t maxSize ) const;

    bool operator < ( const DataRow &other ) const;

public:
    QByteArray cells[DATA_COLS_COUNT];

};


class StorageEngine
{
public:
    StorageEngine( const QString &file );
    ~StorageEngine();

    bool setPassword( const QString &password );

    bool readDbFile();
    bool writeDbFile();

    QString getError();

public:
    std::multiset<DataRow> data;

private:
    bool readFileContent( QByteArray *dst );
    bool writeFileContent( const QByteArray &buf );

    bool encryptData( QByteArray *buf );
    bool decryptData( QByteArray *buf );

private:
    QString           dbFileName;
    QString           errorDescription;
    QByteArray        key;

};

#endif // STORAGE_ENGINE_H
