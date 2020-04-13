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

#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QtCore/QVector>
#include <QtGui/QMainWindow>
#include <QtGui/QPlainTextEdit>
#include <QtGui/QTableWidget>
#include <QtGui/QDialogButtonBox>

#define WINDOW_ICON_PATH "/home/crypton/progs/ds_passkeeper.svg"

class StorageEngine;


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow( const QString &title, StorageEngine *storage );

private:
    void closeEvent( QCloseEvent *event );

    void loadTableContent();
    bool save();

private slots:
    void closeButtonEvent( QAbstractButton *button );
    void filterTable( const QString &keyWord );

    void editCellEvent( int row, int column );
    void changeCellEvent( int newRow, int newCol, int oldRow, int oldCol );
    void tableContextMenuEvent( const QPoint &pos );

    void deleteRow( bool = false );
    void randomizeCell( bool = false );

private:
    StorageEngine *storageEngine;
    QTableWidget *mainTable;
    QPlainTextEdit *commentEdit;
    QDialogButtonBox *closeButtonBox;

    QVector<QString> commentStorage;
    bool dataChanged;
    int curPassRandMode;

};

#endif // MAIN_WINDOW_H
