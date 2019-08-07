/* Copyright (c) 2010-2019, AOYAMA Kazuharu
 * All rights reserved.
 *
 * This software may be used and distributed according to the terms of
 * the New BSD License, which is incorporated herein by reference.
 */

#include <QDir>
#include <QList>
#include <QMapIterator>
#include <QPluginLoader>
#include <QMutex>
#include <QMutexLocker>
#include <TWebApplication>
#include <TSessionStorePlugin>
#include "tsessionstorefactory.h"
#include "tsessionsqlobjectstore.h"
#include "tsessioncookiestore.h"
#include "tsessionfilestore.h"
#include "tsessionredisstore.h"
#include "tsessionmongostore.h"
#include "tsystemglobal.h"
#include <QJsonArray>
#include <QJsonObject>

const QString COOKIE_SESSION_KEY    = TSessionCookieStore().key().toLower();
const QString SQLOBJECT_SESSION_KEY = TSessionSqlObjectStore().key().toLower();
const QString FILE_SESSION_KEY      = TSessionFileStore().key().toLower();
const QString REDIS_SESSION_KEY     = TSessionRedisStore().key().toLower();
const QString MONGODB_SESSION_KEY   = TSessionMongoStore().key().toLower();

static QMutex mutex;
static QMap<QString, TSessionStoreInterface*> *sessIfMap = nullptr;


/*!
  \class TSessionStoreFactory
  \brief The TSessionStoreFactory class creates TSessionStore objects.
*/

static void cleanup()
{
    QMutexLocker locker(&mutex);

    if (sessIfMap) {
        for (QMapIterator<QString, TSessionStoreInterface*> it(*sessIfMap); it.hasNext(); )  {
            it.next();
            delete it.value();
        }
        delete sessIfMap;
        sessIfMap = nullptr;
    }
}

/*!
  Returns the list of valid keys, i.e.\ the available session stores.
*/
QStringList TSessionStoreFactory::keys()
{
    QStringList ret;

    loadPlugins();
    ret << COOKIE_SESSION_KEY
        << SQLOBJECT_SESSION_KEY
        << FILE_SESSION_KEY
        << REDIS_SESSION_KEY
        << MONGODB_SESSION_KEY
        << sessIfMap->keys();

    return ret;
}

/*!
  Creates and returns a TSessionStore object that matches the given key,
  or returns 0 if no matching session store is found.
*/
TSessionStore *TSessionStoreFactory::create(const QString &key)
{
    loadPlugins();
    TSessionStore *ret = nullptr;

    QString k = key.toLower();
    if (k == COOKIE_SESSION_KEY) {
        static TSessionCookieStore cookieStore;
        ret = &cookieStore;
    } else if (k == SQLOBJECT_SESSION_KEY) {
        static TSessionSqlObjectStore sqlObjectStore;
        ret = &sqlObjectStore;
    } else if (k == FILE_SESSION_KEY) {
        static TSessionFileStore fileStore;
        ret = &fileStore;
    } else if (k == REDIS_SESSION_KEY) {
        static TSessionRedisStore redisStore;
        ret = &redisStore;
    } else if (k == MONGODB_SESSION_KEY) {
        static TSessionMongoStore mongoStore;
        ret = &mongoStore;
    } else {
        TSessionStoreInterface *ssif = sessIfMap->value(k);
        if (ssif) {
            ret = ssif->create(key);
        }
    }

    return ret;
}

/*!
  Destroys the \a store, assuming it is of the \a key.
 */
void TSessionStoreFactory::destroy(const QString &key, TSessionStore *store)
{
    if (!store) {
        return;
    }

    QString k = key.toLower();
    if (k == COOKIE_SESSION_KEY) {
        // do nothing
    } else if (k == SQLOBJECT_SESSION_KEY) {
        // do nothing
    } else if (k == FILE_SESSION_KEY) {
        // do nothing
    } else if (k == REDIS_SESSION_KEY) {
        // do nothing
    } else if (k == MONGODB_SESSION_KEY) {
        // do nothing
    } else {
        TSessionStoreInterface *ssif = sessIfMap->value(k);
        if (ssif) {
            ssif->destroy(key, store);
        }
    }
}


/*!
  Loads session store plugins in the plugin directory.
*/
void TSessionStoreFactory::loadPlugins()
{
    if (!sessIfMap) {
        QMutexLocker locker(&mutex);
        if (sessIfMap) {
            return;
        }

        sessIfMap = new QMap<QString, TSessionStoreInterface*>();
        qAddPostRoutine(cleanup);

        QDir dir(Tf::app()->pluginPath());
        const QStringList list = dir.entryList(QDir::Files);
        for (auto &file : list) {
            QPluginLoader loader(dir.absoluteFilePath(file));

            tSystemDebug("plugin library for session store: %s", qPrintable(loader.fileName()));
            if (!loader.load()) {
                tSystemError("plugin load error: %s", qPrintable(loader.errorString()));
                continue;
            }

            TSessionStoreInterface *iface = qobject_cast<TSessionStoreInterface *>(loader.instance());
            if ( iface ) {
                const QVariantList array = loader.metaData().value("MetaData").toObject().value("Keys").toArray().toVariantList();
                for (auto &k : array) {
                    QString key = k.toString().toLower();
                    tSystemInfo("Loaded session store plugin: %s", qPrintable(key));
                    sessIfMap->insert(key, iface);
                }
            }
        }
    }
}
