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

#include "MainWindow.h"

#include <QtCore/QVariant>
#include <QtGui/QApplication>
#include <QtGui/QMenu>
#include <QtGui/QAction>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtGui/QHeaderView>
#include <QtGui/QButtonGroup>
#include <QtGui/QHBoxLayout>
#include <QtGui/QVBoxLayout>
#include <QtGui/QSplitter>
#include <QtGui/QMessageBox>

#include "StorageEngine.h"
#include "Randomizer.h"


#define DATA_COLUMN_COUNT    3
#define QUICK_SEARCH_COLUMNS 2
#define COMMENT_CELL_INDEX   DATA_COLUMN_COUNT

#define SHORTCUT_DELETE_ROW  "del"
#define SHORTCUT_RANDOMIZE   "Ctrl+R"

enum PasswordRandomMode
{
    PRM_PIN_4 = 0,
    PRM_PASS_8,
    PRM_PASS_12,
    PRM_PASS_16,
    PRM_PASS_32,
    PRM_KEY_128,
    PRM_KEY_192,
    PRM_KEY_256,
};

#define DEFAULT_PASSWORD_TYPE   PRM_PASS_12
#define RANDOM_NAMELEN_RANGE    2, 5

static const char *dataColumnHeaders[DATA_COLUMN_COUNT] = { "Service", "Login", "Password" };
static const char *randomDomainSet[] = { ".com", ".net", ".org", ".info", "" };


MainWindow::MainWindow( const QString &title, StorageEngine *storage )
: storageEngine( storage )
, dataChanged( false )
, curPassRandMode( DEFAULT_PASSWORD_TYPE )
{
    setWindowTitle( title );

    QWidget *centralWidget = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout( centralWidget );

    QLineEdit *searchBar = new QLineEdit( this );
    searchBar->setPlaceholderText( "Quick search" );

    QLabel *searchLabel = new QLabel( this );
    QIcon searchIcon( QIcon::fromTheme( "edit-find" ) );
    if( searchIcon.isNull() )
    {
        searchLabel->setText( "Search:" );
    }
    else
    {
        const int searchBarHeight = searchBar->height();
        searchLabel->setPixmap( searchIcon.pixmap( searchBarHeight, searchBarHeight ) );
    }

    QHBoxLayout *searchLayout = new QHBoxLayout();
    searchLayout->addWidget( searchLabel );
    searchLayout->addWidget( searchBar );
    mainLayout->addLayout( searchLayout );

    QSplitter *splitter = new QSplitter( this );
    splitter->setOrientation( Qt::Horizontal );
    mainLayout->addWidget( splitter );

    QSizePolicy sizePolicy1x( QSizePolicy::Expanding, QSizePolicy::Expanding );
    sizePolicy1x.setHorizontalStretch( 1 );
    QSizePolicy sizePolicy2x( QSizePolicy::Expanding, QSizePolicy::Expanding );
    sizePolicy2x.setHorizontalStretch( 2 );

    mainTable = new QTableWidget( splitter );
    mainTable->setSizePolicy( sizePolicy2x );
    mainTable->setEditTriggers( QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked );
    mainTable->setSelectionMode( QAbstractItemView::SingleSelection );
    mainTable->setVerticalScrollMode( QAbstractItemView::ScrollPerPixel );
    mainTable->setHorizontalScrollMode( QAbstractItemView::ScrollPerPixel );
    mainTable->setAlternatingRowColors( true );
    mainTable->setSortingEnabled( false );
    mainTable->setWordWrap( false );
    mainTable->horizontalHeader()->setVisible( true );
    mainTable->verticalHeader()->setVisible( false );
    mainTable->horizontalHeader()->setDefaultSectionSize( 250 );
    mainTable->horizontalHeader()->setMinimumSectionSize( 75 );
    mainTable->horizontalHeader()->setStretchLastSection( true );
    mainTable->setContextMenuPolicy( Qt::CustomContextMenu );
    splitter->addWidget( mainTable );

    QAction *deleteAction = new QAction( mainTable );
    deleteAction->setShortcut( QString( SHORTCUT_DELETE_ROW ) );
    mainTable->addAction( deleteAction );

    QAction *randomizeAction = new QAction( mainTable );
    randomizeAction->setShortcut( QString( SHORTCUT_RANDOMIZE ) );
    mainTable->addAction( randomizeAction );

    commentEdit = new QPlainTextEdit( splitter );
    commentEdit->setSizePolicy( sizePolicy1x );
    splitter->addWidget( commentEdit );

    closeButtonBox = new QDialogButtonBox( this );
    closeButtonBox->setStandardButtons( QDialogButtonBox::Apply | QDialogButtonBox::Discard );
    mainLayout->addWidget( closeButtonBox );

    setCentralWidget( centralWidget );

    loadTableContent();

    QMetaObject::connectSlotsByName( this );
    connect( searchBar, SIGNAL(textEdited(const QString&)),
             this, SLOT(filterTable(const QString&)) );
    connect( mainTable, SIGNAL(cellChanged(int, int)),
             this, SLOT(editCellEvent(int, int)) );
    connect( mainTable, SIGNAL(currentCellChanged(int, int, int, int)),
             this, SLOT(changeCellEvent(int, int, int, int)) );
    connect( mainTable, SIGNAL(customContextMenuRequested(const QPoint&)),
             this, SLOT(tableContextMenuEvent(const QPoint&)) );
    connect( deleteAction, SIGNAL(triggered(bool)),
             this, SLOT(deleteRow(bool)) );
    connect( randomizeAction, SIGNAL(triggered(bool)),
             this, SLOT(randomizeCell(bool)) );
    connect( closeButtonBox, SIGNAL(clicked(QAbstractButton*)),
             this, SLOT(closeButtonEvent(QAbstractButton*)) );
}


void MainWindow::closeEvent( QCloseEvent *event )
{
    if( dataChanged || commentEdit->document()->isModified() )
    {
        QMessageBox message( QMessageBox::Warning,
                             "Warning: unsaved data",
                             "There are pending changes. Save them before ext?",
                             QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                             QApplication::activeWindow() );
        const int option = message.exec();

        if( QMessageBox::Cancel == option || ( QMessageBox::Yes == option && !save() ) )
        {
            event->ignore();
            return;
        }
    }
}


void MainWindow::loadTableContent()
{
    mainTable->setColumnCount( 3 );
    mainTable->setRowCount( storageEngine->data.size() + 1 );
    commentStorage.reserve( storageEngine->data.size() );

    for( int col = 0; col < DATA_COLUMN_COUNT; ++col )
        mainTable->setHorizontalHeaderItem( col, new QTableWidgetItem( dataColumnHeaders[col] ) );

    int curRowIndex = 0;
    for( std::multiset<DataRow>::iterator i = storageEngine->data.begin(); i != storageEngine->data.end(); ++i )
    {
        for( int col = 0; col < DATA_COLUMN_COUNT; ++col )
            mainTable->setItem( curRowIndex, col, new QTableWidgetItem( QString::fromUtf8( i->cells[col] ) ) );

        commentStorage.push_back( QString::fromUtf8( i->cells[COMMENT_CELL_INDEX] ) );
        curRowIndex++;
    }

    storageEngine->data.clear();

    // Since signals are not connected to the slots, call slot explicitly
    mainTable->setCurrentCell( 0, 0 );
    changeCellEvent( 0, 0, -1, -1 );
}


bool MainWindow::save()
{
    if( commentEdit->document()->isModified() )
    {
        const int curRow = mainTable->currentRow();
        if( curRow >= 0 && curRow < commentStorage.size() )
            commentStorage[curRow] = commentEdit->toPlainText();

        commentEdit->document()->setModified( false );
    }

    const int rowCount = mainTable->rowCount() - 1;

    storageEngine->data.clear();

    for( int row = 0; row < rowCount; ++row )
    {
        DataRow dataEntry;

        for( int col = 0; col < DATA_COLUMN_COUNT; ++col )
        {
            QTableWidgetItem *item = mainTable->item( row, col );
            if( NULL != item )
                dataEntry.cells[col] = item->text().toUtf8();
        }

        if( commentStorage.size() > row )
            dataEntry.cells[COMMENT_CELL_INDEX] = commentStorage[row].toUtf8();

        storageEngine->data.insert( dataEntry );
    }

    if( !storageEngine->writeDbFile() )
    {
        QMessageBox message( QMessageBox::Critical,
                             "Error saving data",
                             storageEngine->getError(),
                             QMessageBox::Ok,
                             QApplication::activeWindow() );
        message.exec();

        return false;
    }

    dataChanged = false;
    return true;
}


void MainWindow::closeButtonEvent( QAbstractButton *button )
{
    if( closeButtonBox->buttonRole( button ) == QDialogButtonBox::ApplyRole )
    {
        if( !save() )
            return;
    }
    else // Discard button
    {
        commentEdit->document()->setModified( false );
        dataChanged = false;
    }

    close();
}


void MainWindow::filterTable( const QString &keyWord )
{
    const QString keyWordUpper = keyWord.toUpper();
    const int maxRaws = mainTable->rowCount();
    int firstVisible = -1;

    for( int row = 0; row < maxRaws - 1; ++row )
    {
        bool match = false;
        for( int col = 0; col < QUICK_SEARCH_COLUMNS; ++col )
        {
            QTableWidgetItem *item = mainTable->item( row, col );
            if( NULL != item && item->text().toUpper().contains( keyWordUpper ) )
            {
                match = true;
                break;
            }
        }

        mainTable->setRowHidden( row, !match );
        if( match && firstVisible < 0 )
            firstVisible = row;
    }

    mainTable->setCurrentCell( (maxRaws + firstVisible) % maxRaws, 0 );
}


void MainWindow::editCellEvent( int row, int column )
{
    if( mainTable->rowCount() - 1 == row )
    {
        commentStorage.resize( row + 1 );
        mainTable->setRowCount( row + 2 );
        commentEdit->setEnabled( true );
    }

    dataChanged = true;
}


void MainWindow::changeCellEvent( int newRow, int newCol, int oldRow, int oldCol )
{
    if( newRow == oldRow )
        return;

    if( oldRow >= 0 && oldRow < commentStorage.size()
        && commentEdit->document()->isModified() )
    {
        commentStorage[oldRow] = commentEdit->toPlainText();
        dataChanged = true;
    }

    if( newRow >= 0 && newRow < commentStorage.size() )
    {
        commentEdit->document()->setPlainText( commentStorage[newRow] );
        commentEdit->document()->setModified( false );
        commentEdit->setEnabled( true );
    }
    else
    {
        commentEdit->document()->clear();
        commentEdit->setEnabled( false );
    }
}


void MainWindow::tableContextMenuEvent( const QPoint &pos )
{
    const int curRow = mainTable->currentRow();
    if( curRow < 0 || curRow >= mainTable->rowCount() - 1 )
        return;

    QMenu ctxMenu;

    QAction *deleteAction = ctxMenu.addAction( "Delete row" );
    deleteAction->setShortcut( QString( SHORTCUT_DELETE_ROW ) );
    deleteAction->setData( -1 );

    ctxMenu.addSeparator();

    // Different randomizer options for each data column
    switch( mainTable->currentColumn() )
    {
        case 0: // Service name
                ctxMenu.addAction( "I'm feeling lucky" )->setData( 0 );
            break;

        case 1: // User login
            {
                QAction *randAction = ctxMenu.addAction( "Randomize" );
                randAction->setShortcut( QString( SHORTCUT_RANDOMIZE ) );
                randAction->setData( 0 );
            }
            break;

        case 2: // Password
            {
                QMenu *subMenu = ctxMenu.addMenu( "Generate" );
                subMenu->menuAction()->setShortcut( QString( SHORTCUT_RANDOMIZE ) );
                subMenu->addAction( "4-digit PIN (Bank cards, SIM-cards, etc.)" )->setData( PRM_PIN_4 );
                subMenu->addAction( "8-char password (Non-important accounts, Guest WiFi, etc.)" )
                    ->setData( PRM_PASS_8 );
                subMenu->addAction( "12-char password (Web sites, Online services)" )->setData( PRM_PASS_12 );
                subMenu->addAction( "16-char password (Local computer accounts)" )->setData( PRM_PASS_16 );
                subMenu->addAction( "32-char password (Disk encryption, Private WiFi, etc.)" )
                    ->setData( PRM_PASS_32 );
                subMenu->addAction( "128-bit key in hex" )->setData( PRM_KEY_128 );
                subMenu->addAction( "192-bit key in hex" )->setData( PRM_KEY_192 );
                subMenu->addAction( "256-bit key in hex" )->setData( PRM_KEY_256 );
            }
            break;

        default:
            return;
    }

    QAction* selectedAction = ctxMenu.exec( mainTable->mapToGlobal( pos ) );
    if( NULL == selectedAction )
        return;

    const int option = selectedAction->data().toInt();
    if( -1 == option )
        deleteRow();
    else
    {
        if( mainTable->currentColumn() == 2 )
            curPassRandMode = option;

        randomizeCell();
    }
}


void MainWindow::deleteRow( bool )
{
    const int row = mainTable->currentRow();
    if( !mainTable->hasFocus() || row < 0 || row >= mainTable->rowCount() - 1 )
        return;

    QString rowName;

    QTableWidgetItem *item = mainTable->item( row, 1 );
    if( NULL != item )
        rowName = item->text();

    item = mainTable->item( row, 0 );
    if( NULL != item )
    {
        if( !rowName.isEmpty() && !item->text().isEmpty() )
            rowName += QChar( '@' );

        rowName += item->text();
    }

    QMessageBox message( QMessageBox::Question, "Confirmation",
                         QString( "Delete \"" ) + rowName + "\" ?",
                         QMessageBox::Yes | QMessageBox::Cancel,
                         QApplication::activeWindow() );
    if( message.exec() != QMessageBox::Yes )
        return;

    if( mainTable->model()->removeRow( row ) )
        commentStorage.remove( row );
}


void MainWindow::randomizeCell( bool )
{
    const int row = mainTable->currentRow();
    if( !mainTable->hasFocus() || row < 0 || row >= mainTable->rowCount() - 1 )
        return;

    const int col = mainTable->currentColumn();
    std::string randomized;

    if( 2 == col )
    {
        switch( curPassRandMode )
        {
            case PRM_PIN_4:   randomized = Randomizer::makePin( 4 );       break;
            case PRM_PASS_8:  randomized = Randomizer::makePassword( 8 );  break;
            case PRM_PASS_12: randomized = Randomizer::makePassword( 12 ); break;
            case PRM_PASS_16: randomized = Randomizer::makePassword( 16 ); break;
            case PRM_PASS_32: randomized = Randomizer::makePassword( 32 ); break;
            case PRM_KEY_128: randomized = Randomizer::makeHexBlock( 16 ); break;
            case PRM_KEY_192: randomized = Randomizer::makeHexBlock( 24 ); break;
            case PRM_KEY_256: randomized = Randomizer::makeHexBlock( 32 ); break;
            default: return;
        }
    }
    else
    {
        randomized =  Randomizer::makeName( RANDOM_NAMELEN_RANGE );

        if( 0 == col )
        {
            const int domainNo = Randomizer::makeNumber( sizeof(randomDomainSet) / sizeof(randomDomainSet[0]) - 1 );
            randomized.append( randomDomainSet[domainNo] );
        }
    }

    if( NULL == mainTable->item( row, col ) )
        mainTable->setItem( row, col, new QTableWidgetItem( QString::fromStdString( randomized ) ) );
    else
        mainTable->item( row, col )->setText( QString::fromStdString( randomized ) );
}
