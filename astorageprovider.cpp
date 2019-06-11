#include "astorageprovider.h"
#include <QDir>
#include <QFile>
#include <QSqlQueryModel>
AStorageProvider::AStorageProvider(QString root_path, QObject *parent) : QObject(parent)
{
    mData = new APrivateStorageProvider(root_path);
}


bool AStorageProvider::openDB(QString name, QString db_type)
{

    if(mData != nullptr){
        if(name.compare(mData->mDatabaseName) == 0)
        {
            return true;
        }
        mData->closeDB();
    }
    QString current_root = mData->mRootPath;
    if(db_type.toLower().compare("qsqlite") == 0){
        mData.reset();
        auto  db =new APrivateSqliteStorageProvider(current_root);
        mData = &(*db);
        mData->mDatabaseName = name;
        QDir root_dir(mData->mRootPath);
        if(!root_dir.exists()){
            root_dir.mkpath(root_dir.absolutePath());
        }
        quint8 ret_val = mData->openDB(name,db_type);
        if(ret_val == 0){
            auto schema = mData->loadSchema();
            mData->mSchema = schema;
            if(schema.keys().count() <= 1){
                emit databaseNeedsInitialized();
            } else {
                emit databaseOpen(current_root, name);
            }
            return true;
        }
    }
    return false;
}

void AStorageProvider::closeDB()
{
    mData->closeDB();
}

ASqlCreateTable AStorageProvider::createTable(QString name, bool temporary)
{
    return ASqlCreateTable(name,temporary);
}

QString AStorageProvider::getIndexColumnName()
{
    QJsonObject& schema = mData->mSchema;
    QStringList keys = schema.keys();
    QString name;
    for(auto key : keys){
        QJsonObject table = schema.value(key).toObject();
        auto columns = table.value("columns").toObject();
        auto ckeys = columns.keys();
        for(auto ckey : ckeys){
            auto column = columns.value(ckey).toObject();
            if(column.contains("primary")){
                name = ckey;
                break;
            }
        }

    }
    return name;
}

QVariantMap AStorageProvider::getLastRecord(QString table_name)
{
    QVariantMap vm;
    QString col = getIndexColumnName();
    auto result = executeQuery(selectFrom(table_name).max(col).end());
    if(result && result->rowCount() > 0){
        auto record = result->record(0);
        quint64 primary = record.value(0).toULongLong();
        QString where("'"+col + " == " + QString::number(primary) + "'");
        auto inner_result = executeQuery(selectFrom(table_name).where(where).end());
        if(inner_result && inner_result->rowCount() > 0){
            auto last_record = inner_result->record(0);
            int columns = last_record.count();
            for(int cindex(0); cindex < columns; ++cindex){
                 vm.insert(last_record.fieldName(cindex), last_record.value(cindex));
            }
        }
    }
    return vm;
}


QStringList AStorageProvider::getNonNullValueNames() const
{
    QStringList names;
    QJsonObject& schema = mData->mSchema;
    QStringList keys = schema.keys();
    for(auto key : keys){
        QJsonObject table = schema.value(key).toObject();
        auto columns = table.value("columns").toObject();
        auto ckeys = columns.keys();
        for(auto ckey : ckeys){
            auto column = columns.value(ckey).toObject();
            if(column.contains("null")){
                bool nullable = column.value("null").toBool();
                if(!nullable){
                    names.append(key);
                }
            }
        }
    }
    return names;
}


ASqlInsert AStorageProvider::insertInto(QString table_name)
{
    return ASqlInsert(table_name);
}

ASqlSelect AStorageProvider::selectFrom(QString table_name) const
{
    return ASqlSelect(table_name);
}

ASqlSelect AStorageProvider::selectFrom(ASqlSelect inner_sql) const
{
    return ASqlSelect(inner_sql);
}

bool AStorageProvider::errored()
{
    return mData->mError;
}

QSharedPointer<QSqlQueryModel> AStorageProvider::executeQuery(const ASqlStatement &statement)
{
    // try to build the query based on the type
    const QJsonObject &schema = statement.query();
    //int type = schema.value("type").toInt();
    QSharedPointer<QSqlQueryModel> model(nullptr);
    switch(mData->mType)
    {
        case 1: // Sqlite
            {
                auto p = static_cast<APrivateSqliteStorageProvider*>(mData.data());
                model = p->performQuery(schema);
                break;
            }
            default: break;
    }
   /* if(ret_val == 0){
        emit querySuceeded(mData->getQueryName(type),schema);
    }
    else{
        emit failedToExecuteQuery(mData->getQueryName(type), schema, 2);
    }*/
    return model;
}

QString AStorageProvider::storagePath() const
{
    QString ret;
    if(mData.data()){
        ret = mData->mRootPath;
    }
    return ret;
}

bool AStorageProvider::setPath(QString path)
{
    bool success(false);
    if(mData.data()){
        mData->mRootPath = path;
    }
    return success;
}

QString AStorageProvider::name() const
{
    return mData->mDatabaseName;
}
QString AStorageProvider::rootPath() const
{
    return mData->mRootPath;
}

const QJsonObject& AStorageProvider::schema() const
{
    return mData->mSchema;
}

AStorageProvider::~AStorageProvider()
{
    if(mData.data()){
        mData.reset();
    }
}
