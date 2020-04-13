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
#include "MainWindow.h"

#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>
#include <QtGui/QMessageBox>
#include <QtGui/QInputDialog>
#include <QtGui/QFileDialog>
#include <QtCore/QFile>

#include <stdio.h>
#include <string.h>


#define APPLICATION_NAME    "Damn Simple Password Keeper"

#define PASSDB_FILE_SUFFIX  "passdb"

enum AppCommand
{
    CMD_NOTHING = 0x00,
    CMD_OPEN    = 0x01,
    CMD_NEWPASS = 0x02,
    CMD_EDIT    = 0x04,
    CMD_HELP    = 0x08,
    CMD_FILEDLG = 0x10
};


static void help( const char *programName )
{
    printf( "Usage:\n\t%s [filename]\n"
            "\t\tSimplified usage: open file if specified, create new one otherwise\n\n"
            "\t%s open <filename> [Qt options]\n"
            "\t\tOpen existing password storage\n\n"
            "\t%s new <filename> [Qt options]\n"
            "\t\tCreate new password storage (existing file will be owerwritten!)\n\n"
            "\t%s chpass <filename> [Qt options]\n"
            "\t\tChange master password of existing password storage\n\n",
            programName, programName, programName, programName );
}


static int identifyCommand( const char *cmd )
{
    static const char *helpKeywords[] = {
        "help", "-h", "-help", "--help", "-?", "/?", "\\?"
    };

    if( strcmp( cmd, "open" ) == 0 )   return ( CMD_OPEN | CMD_EDIT );
    if( strcmp( cmd, "new" ) == 0 )    return ( CMD_NEWPASS | CMD_EDIT );
    if( strcmp( cmd, "chpass" ) == 0 ) return ( CMD_OPEN | CMD_NEWPASS );

    for( size_t i = 0; i < sizeof(helpKeywords)/sizeof(helpKeywords[0]); ++i )
        if( strcmp( cmd, helpKeywords[i] ) == 0 )
            return CMD_HELP;

    return CMD_NOTHING;
}


static void errorDialog( const QString &text )
{
    QMessageBox dialog( QMessageBox::Critical, "Fatal error", text, QMessageBox::Ok );
    dialog.exec();
}


static bool passwordDialog( QString *dst, const QString &labelText )
{
    QInputDialog dialog;
    dialog.setLabelText( labelText );
    dialog.setWindowIcon(QIcon(WINDOW_ICON_PATH));
    dialog.setInputMode( QInputDialog::TextInput );
    dialog.setTextEchoMode( QLineEdit::Password );

    QRect screen = QApplication::desktop()->screenGeometry();
    dialog.resize( screen.width() / 2, 0 );

    if( !dialog.exec() )
        return false;

    *dst = dialog.textValue();
    return true;
}


static bool fileDialog( QString *dst, bool newOne )
{
    QFileDialog dialog;
    dialog.setAcceptMode( newOne ? QFileDialog::AcceptSave : QFileDialog::AcceptOpen );
    dialog.setFileMode( newOne ? QFileDialog::AnyFile : QFileDialog::ExistingFile );

    QStringList filters;
    filters << "Password DB files (*." PASSDB_FILE_SUFFIX ")"
            << "Any files (*)";
    dialog.setNameFilters( filters );

    if( newOne )
    {
        dialog.setDefaultSuffix( PASSDB_FILE_SUFFIX );
        dialog.setLabelText( QFileDialog::Accept, "Create" );
    }

    if( !dialog.exec() )
        return false;

    QStringList files = dialog.selectedFiles();
    if( files.length() != 1 )
    {
        errorDialog( "Exactly one file is expected" );
        return false;
    }

    *dst = files[0];
    return true;
}


static bool loadDataFile( StorageEngine *storage )
{
    QString password;
    if( !passwordDialog( &password, "Enter master password:" ) )
        return false;

    if( !storage->setPassword( password ) || !storage->readDbFile() )
    {
        errorDialog( storage->getError() );
        return false;
    }

    return true;
}

static bool askSetNewPassword( StorageEngine *storage )
{
    QString passPrompt = "Enter new master password:";
    QString pass1, pass2;

    while( true )
    {
        if( !passwordDialog( &pass1, passPrompt )
            || !passwordDialog( &pass2, "Enter password again to confirm:" ) )
        {
            return false;
        }

        if( pass1 == pass2 )
            break;

        passPrompt = "Passwords mismatch. Please try again or choose another one:";
    }

    if( !storage->setPassword( pass1 ) )
    {
        errorDialog( storage->getError() );
        return false;
    }

    return true;
}


int main( int argc, char **argv )
{
    int appCommand;
    QString fileName;
    QString appName( APPLICATION_NAME );

    // 1. Parse CLI arguments
    if( argc <= 0 )
    {
        return 1;
    }
    else if( 1 == argc )
    {
        // Simplified usage, no arguments
        appCommand = CMD_FILEDLG | CMD_NEWPASS | CMD_EDIT;
    }
    else
    {
        appCommand = identifyCommand( argv[1] );

        if( appCommand & CMD_HELP )
        {
            help( argv[0] );
            return 0;
        }
        else if( CMD_NOTHING != appCommand )
        {
            if( argc > 2 )
                fileName = QString::fromLocal8Bit( argv[2] );
            else
                appCommand |= CMD_FILEDLG;
        }
        else
        {
            fileName = QString::fromLocal8Bit( argv[1] );
            if( 2 == argc && QFile::exists( fileName ) )
            {
                // Simplified usage, file specified
                appCommand = CMD_OPEN | CMD_EDIT;
            }
            else
            {
                help( argv[0] );
                return 1;
            }
        }
    }

    // 2. Initialize the application
    QApplication app( argc, argv );
    app.setApplicationName( appName );

    if( (appCommand & CMD_FILEDLG) != 0
        && !fileDialog( &fileName, (appCommand & CMD_OPEN) == 0 ) )
    {
        // User has cancelled the file dialog or error occured
        return 1;
    }

    StorageEngine storage( fileName );

    // 3. Execute commands
    if( (appCommand & CMD_OPEN) != 0 && !loadDataFile( &storage ) )
        return 1;

    if( (appCommand & CMD_NEWPASS) != 0 && !askSetNewPassword( &storage ) )
        return 1;

    if( (appCommand & CMD_EDIT) != 0 )
    {
        // Open editing window. DB will be saved by MainWindow routines
        MainWindow window( appName, &storage );

        QRect screen = QApplication::desktop()->screenGeometry();
        window.setGeometry( screen.width() / 6, screen.height() / 6,
                            screen.width() * 2 / 3, screen.height() * 2 / 3 );

#ifdef Q_WS_X11
        // Set X11 flags to let it choose correct screen and center the window
        window.setAttribute(Qt::WA_Moved, false);
        window.setAttribute(Qt::WA_X11NetWmWindowTypeDialog, true);
#endif

        window.show();

        return app.exec();
    }
    else
    {
        // No main window, just save database
        if( !storage.writeDbFile() )
        {
            errorDialog( storage.getError() );
            return 1;
        }

        QMessageBox dialog( QMessageBox::NoIcon, "Success",
                            "Password DB updated successfully", QMessageBox::Ok );
        dialog.exec();

        return 0;
    }
}
