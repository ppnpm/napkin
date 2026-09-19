#include "SingleInstance.h"
#include "Paths.h"

#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <QLocalSocket>

namespace napkin {

SingleInstance::SingleInstance(QObject* parent) : QObject(parent)
{
    // Not a bare name: Qt would put that in a shared temp directory when
    // XDG_RUNTIME_DIR is unset, world-connectable, where any local user can
    // create it first and stop Napkin starting at all. Under the 0700 data
    // directory it is ours alone.
#ifdef Q_OS_WIN
    // On Windows a local server is a named pipe, which lives in a flat,
    // machine-wide namespace rather than in a directory. The pipe is named after
    // the data directory, which contains the user's profile path, so two users
    // (or a test profile) get different pipes; UserAccessOption puts an ACL on
    // it so only this user can connect.
    key_ = QStringLiteral("io.github.sudomonas.Napkin-")
         + QString::fromLatin1(QCryptographicHash::hash(paths::dataDir().toUtf8(),
                                                        QCryptographicHash::Sha256)
                                   .toHex().left(32));
#else
    key_ = paths::dataDir() + QStringLiteral("/napkin.sock");
#endif
    server_.setSocketOptions(QLocalServer::UserAccessOption);

    connect(&server_, &QLocalServer::newConnection, this, [this] {
        if (auto* c = server_.nextPendingConnection()) {
            connect(c, &QLocalSocket::disconnected, c, &QLocalSocket::deleteLater);
            emit raiseRequested();
        }
    });
}

bool SingleInstance::acquire()
{
    // A live primary answers immediately.
    QLocalSocket probe;
    probe.connectToServer(key_);
    if (probe.waitForConnected(300)) {
        probe.write("raise");
        probe.waitForBytesWritten(300);
        probe.disconnectFromServer();
        return false;
    }

    // Nobody answered. Any socket file still present is stale — a previous
    // Napkin was killed before it could clean up — so reclaim it.
    if (!server_.listen(key_)) {
        QLocalServer::removeServer(key_);
        if (!server_.listen(key_)) return true;  // degrade to running anyway
    }
    return true;
}

}  // namespace napkin
