#include "SingleInstance.h"
#include "Paths.h"

#include <QDir>
#include <QFile>
#include <QLocalSocket>
#include <unistd.h>

namespace napkin {

SingleInstance::SingleInstance(QObject* parent) : QObject(parent)
{
    // Not a bare name: Qt would put that in a shared temp directory when
    // XDG_RUNTIME_DIR is unset, world-connectable, where any local user can
    // create it first and stop Napkin starting at all. Under the 0700 data
    // directory it is ours alone.
    key_ = paths::dataDir() + QStringLiteral("/napkin.sock");
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
