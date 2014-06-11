/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/


#include <QtTest/QtTest>
#include <QtGui>
#include <QtWidgets>
#include <QtCore>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <qdebug.h>

#include "../../network-settings.h"
#include "../../minihttpserver.h"

class tst_QNetworkAccessManager_And_QProgressDialog : public QObject
{
    Q_OBJECT
public:
    tst_QNetworkAccessManager_And_QProgressDialog();

    QByteArray testData;
private slots:
    void downloadCheck();
    void downloadCheck_data();
};

class DownloadCheckWidget : public QWidget
{
    Q_OBJECT
public:
    DownloadCheckWidget(int port, QWidget *parent = 0) :
        QWidget(parent), lateReadyRead(true), zeroCopy(false),
        progressDlg(this), netmanager(this)
    {
        progressDlg.setRange(1, 100);
        QUrl url("http://localhost");
        url.setPort(port);
        QMetaObject::invokeMethod(this, "go", Qt::QueuedConnection, Q_ARG(QUrl, url));
    }
    bool lateReadyRead;
    bool zeroCopy;
public slots:
    void go(const QUrl &url)
    {
        QNetworkRequest request(url);
        if (zeroCopy)
            request.setAttribute(QNetworkRequest::MaximumDownloadBufferSizeAttribute, 10*1024*1024);

        QNetworkReply *reply = netmanager.get(request);
        connect(reply, SIGNAL(downloadProgress(qint64,qint64)),
                this, SLOT(dataReadProgress(qint64,qint64)));
        connect(reply, SIGNAL(readyRead()),
                this, SLOT(dataReadyRead()));
        connect(reply, SIGNAL(finished()), this, SLOT(finishedFromReply()));

        progressDlg.exec();
    }
    void dataReadProgress(qint64 done, qint64 total)
    {
        QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
        Q_UNUSED(reply);
        progressDlg.setMaximum(total);
        progressDlg.setValue(done);
    }
    void dataReadyRead()
    {
        QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
        Q_UNUSED(reply);
        lateReadyRead = true;
    }
    void finishedFromReply()
    {
        QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
        lateReadyRead = false;
        reply->deleteLater();
        QTestEventLoop::instance().exitLoop();
    }

private:
    QProgressDialog progressDlg;
    QNetworkAccessManager netmanager;
};

#define TESTDATASIZE    33554176        /* 32 MB - 256 */
tst_QNetworkAccessManager_And_QProgressDialog::tst_QNetworkAccessManager_And_QProgressDialog()
{
    static const char httpHeader[] =
            "HTTP/1.1 200 OK\r\n"
            "Connection: keep-alive\r\n"    // we'll actually close, but it's fine
            "Content-Type: text/plain\r\n"
            "Content-Length: " QT_STRINGIFY(TESTDATASIZE) "\r\n"
            "\r\n";
    testData.fill('a', TESTDATASIZE + sizeof httpHeader - 1);
    memcpy(testData.data(), httpHeader, sizeof httpHeader - 1);
}

void tst_QNetworkAccessManager_And_QProgressDialog::downloadCheck_data()
{
    QTest::addColumn<bool>("useZeroCopy");
    QTest::newRow("with-zeroCopy") << true;
    QTest::newRow("without-zeroCopy") << false;
}

void tst_QNetworkAccessManager_And_QProgressDialog::downloadCheck()
{
    QFETCH(bool, useZeroCopy);

    MiniHttpServer server(testData);

    DownloadCheckWidget widget(server.serverPort());
    widget.zeroCopy = useZeroCopy;
    widget.show();
    // run and exit on finished()
    QTestEventLoop::instance().enterLoop(10);
    QVERIFY(!QTestEventLoop::instance().timeout());
    // run some more to catch the late readyRead() (or: to not catch it)
    QTestEventLoop::instance().enterLoop(1);
    QVERIFY(QTestEventLoop::instance().timeout());
    // the following fails when a readyRead() was received after finished()
    QVERIFY(!widget.lateReadyRead);
}



QTEST_MAIN(tst_QNetworkAccessManager_And_QProgressDialog)
#include "tst_qnetworkaccessmanager_and_qprogressdialog.moc"
