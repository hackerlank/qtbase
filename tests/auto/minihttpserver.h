/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef MINIHTTPSERVER_H
#define MINIHTTPSERVER_H

#include <QtCore/QPointer>
#include <QtCore/QSemaphore>
#include <QtCore/QThread>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QSslError>
#include <QtNetwork/QSslSocket>
#include <QtTest/QtTest>

#ifndef QT_NO_SSL
inline void setupSslServer(QSslSocket* serverSocket)
{
    QString testDataDir = QFileInfo(QFINDTESTDATA("certs")).absolutePath();
    if (testDataDir.isEmpty())
        testDataDir = QCoreApplication::applicationDirPath();

    serverSocket->setProtocol(QSsl::AnyProtocol);
    serverSocket->setLocalCertificate(testDataDir + "/certs/server.pem");
    serverSocket->setPrivateKey(testDataDir + "/certs/server.key");
}
#endif

// Does not work for POST/PUT!
class MiniHttpServer: public QTcpServer
{
    Q_OBJECT
public:
    QPointer<QTcpSocket> client; // always the last one that was received
    QByteArray dataToTransmit;
    QByteArray receivedData;
    QSemaphore ready;
    bool doClose;
    bool doSsl;
    bool ipv6;
    bool multiple;
    int totalConnections;

    MiniHttpServer(const QByteArray &data, bool ssl = false, QThread *thread = 0, bool useipv6 = false)
        : dataToTransmit(data), doClose(true), doSsl(ssl), ipv6(useipv6),
          multiple(false), totalConnections(0)
    {
        if (useipv6) {
            if (!listen(QHostAddress::AnyIPv6))
                qWarning() << "listen() IPv6 failed" << errorString();
        } else {
            if (!listen(QHostAddress::AnyIPv4))
                qWarning() << "listen() IPv4 failed" << errorString();
        }
        if (thread) {
            connect(thread, SIGNAL(started()), this, SLOT(threadStartedSlot()));
            moveToThread(thread);
            thread->start();
            ready.acquire();
        }
    }

    void setDataToTransmit(const QByteArray &data)
    {
        dataToTransmit = data;
    }

protected:
    void incomingConnection(qintptr socketDescriptor)
    {
        //qDebug() << "incomingConnection" << socketDescriptor << "doSsl:" << doSsl << "ipv6:" << ipv6;
        if (!doSsl) {
            client = new QTcpSocket;
            client->setSocketDescriptor(socketDescriptor);
            connectSocketSignals();
        } else {
#ifndef QT_NO_SSL
            QSslSocket *serverSocket = new QSslSocket;
            serverSocket->setParent(this);
            if (serverSocket->setSocketDescriptor(socketDescriptor)) {
                connect(serverSocket, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(slotSslErrors(QList<QSslError>)));
                setupSslServer(serverSocket);
                serverSocket->startServerEncryption();
                client = serverSocket;
                connectSocketSignals();
            } else {
                delete serverSocket;
                return;
            }
#endif
        }
        client->setParent(this);
        ++totalConnections;
    }

    virtual void reply()
    {
        Q_ASSERT(!client.isNull());
        // we need to emulate the bytesWrittenSlot call if the data is empty.
        if (dataToTransmit.size() == 0)
            QMetaObject::invokeMethod(this, "bytesWrittenSlot", Qt::QueuedConnection);
        else
            client->write(dataToTransmit);
    }
private:
    void connectSocketSignals()
    {
        Q_ASSERT(!client.isNull());
        //qDebug() << "connectSocketSignals" << client;
        connect(client.data(), SIGNAL(readyRead()), this, SLOT(readyReadSlot()));
        connect(client.data(), SIGNAL(bytesWritten(qint64)), this, SLOT(bytesWrittenSlot()));
        connect(client.data(), SIGNAL(error(QAbstractSocket::SocketError)),
                this, SLOT(slotError(QAbstractSocket::SocketError)));
    }

private slots:
#ifndef QT_NO_SSL
    void slotSslErrors(const QList<QSslError>& errors)
    {
        Q_ASSERT(!client.isNull());
        qDebug() << "slotSslErrors" << client->errorString() << errors;
    }
#endif
    void slotError(QAbstractSocket::SocketError err)
    {
        Q_ASSERT(!client.isNull());
        qDebug() << "slotError" << err << client->errorString();
    }

public slots:
    void readyReadSlot()
    {
        Q_ASSERT(!client.isNull());
        receivedData += client->readAll();
        int doubleEndlPos = receivedData.indexOf("\r\n\r\n");

        if (doubleEndlPos != -1) {
            // multiple requests incoming. remove the bytes of the current one
            if (multiple)
                receivedData.remove(0, doubleEndlPos+4);

            reply();
        }
    }

    void bytesWrittenSlot() {
        Q_ASSERT(!client.isNull());
        // Disconnect and delete in next cycle (else Windows clients will fail with RemoteHostClosedError).
        if (doClose && client->bytesToWrite() == 0) {
            disconnect(client, 0, this, 0);
            client->deleteLater();
        }
    }

    void threadStartedSlot()
    {
        ready.release();
    }
};

#endif // MINIHTTPSERVER_H
