#ifndef ASTORAGEPROVIDER_P_H
#define ASTORAGEPROVIDER_P_H
#include <QObject>
#include <QMap>
#include <QPair>
#include <QString>
#include <QObject>
#include <QDebug>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSharedData>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariant>
#include <QVariantList>
#include <QJsonValue>
#include <QSqlQueryModel>
#include <QSqlError>
#include "asql_statement_p.h"
#include <QDateTime>
constexpr const char* SQL_QUERY_NAME_TABLE[] = {"CREATE_TABLE", "ALTER_TABLE","DELETE_TABLE","INSERT_TABLE"};
constexpr const char* SQL_DB_TYPES[] = {"", "QSQLITE","QMYSQL","QPSQL"};
class APrivateStorageProvider : public QSharedData
{

public:
    enum SQL_PROVIDERS{NONE = 0, SQLITE, MYSQL, PSQL};
    QString             mRootPath;
    QString             mDatabaseName;
    QJsonObject         mSchema;
    quint32             mType;
    bool                mError;
    // openDB (if already open) should likely perform the detach
    virtual quint8 clearAndSetRoot(QString root_path){ Q_UNUSED(root_path);return 63;}
    virtual quint8 openDB(QString name, QString db_type){Q_UNUSED(name);Q_UNUSED(db_type);return 63;}
    virtual quint8 alterTable(QString name, QJsonObject schema){ Q_UNUSED(name); Q_UNUSED(schema);return 63; }
    virtual quint8 createTable(const QJsonObject &schema){Q_UNUSED(schema);return 63; }
    virtual quint8 removeTable(QString name){ Q_UNUSED(name);return 63; }
    virtual QString getQueryName(int query)
    {
        if(query >= ASqlStatement::CREATE_TABLE && query <= ASqlStatement::INSERT_TABLE){
            return SQL_QUERY_NAME_TABLE[query];
        }
        return QString("BAD_QUERY");;
    }
    virtual QString dbType(SQL_PROVIDERS db)
    {
        if(db >= SQL_PROVIDERS::SQLITE && db <= SQL_PROVIDERS::PSQL){
            return SQL_DB_TYPES[db];
        }
        return QString("");;
    }
    virtual QSharedPointer<QSqlQueryModel> performQuery(QJsonObject query ){Q_UNUSED(query); return  nullptr;}
    virtual QJsonObject loadSchema(){return QJsonObject();}
    virtual quint8 closeDB(){return 63; }
    APrivateStorageProvider(QString path, quint32 type = 0) : mRootPath(path),mType(type),mError(false) {}
};



class APrivateSqliteStorageProvider : public APrivateStorageProvider
{

public:

    APrivateSqliteStorageProvider(QString path) : APrivateStorageProvider(path, APrivateSqliteStorageProvider::SQLITE)
    { qsrand(QDateTime::currentMSecsSinceEpoch()); }
    ~APrivateSqliteStorageProvider()
    {
        closeDB();
    }

    bool clearAndSetDB(QString root_path, QString database_name = "")
    {
        if(mRootPath.compare(root_path) != 0 ||  mDatabaseName.compare(database_name) != 0){
            mRootPath = root_path;
            if(!mRootPath.endsWith("/"))
                    mRootPath.append("/");
            mDatabaseName = database_name;
            return true;
        }
        return false;
    }
    quint8 openDB(QString name, QString db_type)
    {
        QSqlDatabase db = QSqlDatabase::database(mRootPath+name, false);
        if(db.isValid()) {
            db.close();
            qDebug() << "found existing db and removing it";
            QSqlDatabase::removeDatabase(mRootPath+name);
        }
        QSqlDatabase db2 = QSqlDatabase::addDatabase(db_type, mRootPath + name);
        qDebug() << "Added the Database " << mRootPath+name;
        if(db2.isValid()){
            db2.setHostName("AEMAC");
            db2.setDatabaseName(mRootPath+name);
            db2.setConnectOptions();
            if(db2.open()){
                return 0;
            }
            qDebug() << "Failed to open the database again:";
            qDebug() << db2.lastError();
            return 2;
        }
        return 3; // bad DB or driver Specified
    }
    quint8 createTable(const QJsonObject &schema)
    {
        QString name = schema.value("name").toString();
        if(mSchema.contains(name)){
            return 1; // table already exists
        }
        quint8 reason(0);
        if(createTableFromSchema(schema,reason)){
            mSchema.insert(name,schema);
            updateSchema();
            return 0; // success
        }
        return reason; // failed to create table
    }
    quint8 insertTable(QJsonObject schema)
    {
        QString name = schema.value("name").toString();
        if(mSchema.contains(name)){
            return 1; // table already exists
        }
        quint8 reason(0);
        createInsertTableFromSchema(schema,reason);
        return reason; // failed to create table
    }
    QJsonObject loadSchema()
    {
        QSqlDatabase db = QSqlDatabase::database(mRootPath+mDatabaseName);
        QSqlQuery sql_query(db);
        db.setHostName("AEMAC");
        db.setDatabaseName(mRootPath+mDatabaseName);
        db.setConnectOptions();
        if(db.isOpen() || db.open()){
            sql_query.prepare("SELECT content FROM schema_table WHERE oid = 1");
            if(sql_query.exec() && sql_query.next()){
                    // get the content
                    QString json_string = sql_query.value(0).toString();
                    QJsonDocument doc = QJsonDocument::fromJson(json_string.toLocal8Bit());
                    QJsonObject schema = doc.object();
                    return schema;
            }
            else{ // create schema table
                ASqlStatement s;
                s.setQuery(ASqlStatement::CREATE_TABLE);
                s.setTableName("schema_table");
                s.addColumn("content",QMetaType::QJsonObject);
                s.setColumnValue("content","unique",true);

                QJsonObject schema_table;
                QJsonObject columns;
                columns.insert("content", QMetaType::QJsonObject);
                schema_table.insert("columns", columns);
                schema_table.insert("name", "schema_table");
                quint8 ret_val(-1);
                if(createTableFromSchema(s.query(),ret_val)){
                    mSchema.insert("schema_table",schema_table);
                    insertSchema();
                    return mSchema;
                }
            }
//            db.close();
//            QSqlDatabase::removeDatabase(mRootPath+mDatabaseName);
        }
        return QJsonObject();
    }
    bool insertSchema()
    {
        QSqlDatabase db = QSqlDatabase::database(mRootPath+mDatabaseName);
        db.setHostName("AEMAC");
        db.setDatabaseName(mRootPath+mDatabaseName);
        db.setConnectOptions();
        bool ret_val(false);
        if(db.open()){
            QSqlQuery query(db);
            query.prepare("INSERT INTO schema_table VALUES(?)");
            QJsonDocument doc;
            doc.setObject(mSchema);
            QString json = doc.toJson();
            query.addBindValue(json);
            ret_val = query.exec();
//            db.close();
//            QSqlDatabase::removeDatabase(mRootPath+mDatabaseName);
        }
        return ret_val;
    }
    bool updateSchema()
    {
        QSqlDatabase db = QSqlDatabase::database(mRootPath+mDatabaseName);
        db.setHostName("AEMAC");
        db.setDatabaseName(mRootPath+mDatabaseName);
        db.setConnectOptions();
        bool ret_val(false);
        if(db.open()){
            QSqlQuery query(db);
            query.prepare("UPDATE  schema_table SET content = ? WHERE oid = 1");
            QJsonDocument doc;
            doc.setObject(mSchema);
            QString json = doc.toJson();
            query.addBindValue(json);
            ret_val = query.exec();
            db.commit();
//            db.close();
//            QSqlDatabase::removeDatabase(mRootPath+mDatabaseName);
        }
        return ret_val;
    }


    // Potentially SQlite only functions
    QSharedPointer<QSqlQueryModel> performQuery(QJsonObject query)
    {
        int type = query.value("type").toInt(-1);
        QSharedPointer<QSqlQueryModel> model(nullptr);
        quint8 ret_val(-1);
        switch(type){
            case ASqlStatement::CREATE_TABLE:
            {

                if(createTableFromSchema(query,ret_val)){
                    query.remove("type");
                    mSchema.insert(query.value("name").toString(),query);
                    updateSchema();
                    //closeDB();
                } else{
                    mError = true;
                }
                break;
            }
            case ASqlStatement::INSERT_TABLE:
            {
                if(createInsertTableFromSchema(query,ret_val))
                {
                    model = QSharedPointer<QSqlQueryModel>(nullptr);
                }else {
                    mError = true;
                }
                break;
            }
            case ASqlStatement::SELECT_TABLE:
            {
                model = QSharedPointer<QSqlQueryModel>(createSelectTableFromSchema(query,ret_val));
                if(model == nullptr){
                    mError = true;
                }
                break;
            }
            default: break;
        }
        return model;
    }
protected:
    quint8 closeDB()
    {
        //qDebug() << "Closing " << mRootPath+mDatabaseName;
        QSqlDatabase db = QSqlDatabase::database(mRootPath+mDatabaseName);
        if(db.isValid() && db.isOpen()) {
            db.close();
        }
        QSqlDatabase::removeDatabase(mRootPath+mDatabaseName);
        return 0;
    }
    bool createTableFromSchema(QJsonObject schema, quint8& reason)
    {
        QString name;
        if(schema.contains("name")){
            name =schema.value("name").toString();
        }
        else{
            reason = -1;
            return false;
        }
        if(schema.isEmpty()){
            reason = -2; // empty schema
            return false;
        }

        // Begin creating the query
        QString query = "CREATE ";
        if(schema.contains("temp")){
            query.append(" TEMP ");
        }
        if(schema.contains("schema_name")){
            QString schema_name =schema.value("schema_name").toString();
            if(!schema_name.isEmpty()){
                query.append(schema_name + "."); // must be main, temp, or name of attached db
            }
        }
        query.append("TABLE IF NOT EXISTS ");
        query.append(name);
        query.append(" ( "); // begin column definitions
        auto columns = schema.value("columns").toObject();
        auto keys = columns.keys();
        for(auto column_name: keys){
            QJsonObject column = columns.value(column_name).toObject();
            if(column_name.isEmpty()){
                reason =  -3; // empty column name
                return false;
            }
            query.append(column_name);
            query.append(" ");
            int type = column.value("type").toInt();
            if(column.contains("primary")){
                query.append(" INTEGER PRIMARY KEY ");
            } else{
                query.append( qtTypeToSqliteType(type));
                if(column.contains("not_null")){
                    query.append("NOT NULL ");
                }
            }
            if(column.contains("default")){
                QJsonValue   value = column.value("default");
                QString  string_val =  jvalueToString(value);
                if(!string_val.isEmpty()){
                    query.append(" DEFAULT ");
                    query.append(string_val);
                }
            }
            QString check_str = column.value("check").toString();
            if(!check_str.isEmpty()){
                query.append(" CHECK(");
                query.append(check_str);
                query.append(")");
            }
            if(column_name.compare(keys.last()) != 0){
                query.append(", ");
            }
        }

        query.append(")");
        // actually run the statement and try to create the table.
        reason = 0;
        return sendQueryToDB(query);
    }
    QSqlQueryModel* createSelectTableFromSchema(QJsonObject query_obj, quint8& reason)
    {
        QString name;
        QSqlQueryModel* inner_table(nullptr);
        bool from_is_table_name(true);
        if(query_obj.contains("name")){ // must have a table name provided
            if((from_is_table_name = query_obj.value("name").isString())){
                name = query_obj.value("name").toString();
            }
            else if(query_obj.value("name").isObject()){ // table name might be another select statement
                QJsonObject inner_query = query_obj.value("name").toObject();
                inner_table = createSelectTableFromSchema(inner_query, reason);
                if(inner_table == nullptr){
                    return nullptr;
                }
            }
        }
        else{
            reason = -1;
            return nullptr;
        }
        if(query_obj.isEmpty()){
            reason = -2; // empty schema
            return nullptr;
        }
        QString query("SELECT ");
        if(query_obj.contains("distinct")){
            query.append("DISTINCT ");
        }
        auto columns = query_obj.value("columns").toObject();
        auto keys = columns.keys();
        int column_count = keys.count();
        if(column_count <= 0 ){
            query.append(" * ");
        }
        else{
            for(int index(0); index < column_count; ++index){
                QString column_name = keys.at(index);
                if(column_name.isEmpty()){
                    reason =  -3; // empty column name
                    return nullptr;
                }
                if(index != 0){
                    query.append(", ");
                }
                query.append(column_name);
            }
        }
        query.append( " FROM ");
        query.append(name);
        if(query_obj.contains("where"))
        {
            query.append(" WHERE(");
            QJsonValue where_val = query_obj.value("where");
            if(where_val.isString()){
                query.append(where_val.toString());
            }
            else if(where_val.isObject()) // built using the C++ where statement object
            {

            }
            query.append(")");
        }
        if(inner_table != nullptr)
        {
            QSqlQuery squery = inner_table->query();
            if(squery.exec(query)){
                QSqlQueryModel* sql_model = reinterpret_cast<QSqlQueryModel*>(inner_table);
                sql_model->setQuery(squery);
            }
        }
        else
        {
            QVariantList params;
            inner_table = fetchTableModel(query,params);
        }
        return inner_table;
    }
    bool createInsertTableFromSchema(QJsonObject query_obj, quint8& reason)
    {
        QString name;
        if(query_obj.contains("name")){ // must have a table name provided
            name = query_obj.value("name").toString();
        }
        else{
            reason = -1;
            return false;
        }
        if(query_obj.isEmpty()){
            reason = -2; // empty schema
            return false;
        }
        QString query("INSERT INTO ");
        query.append(name);
        query.append( " ");
        QString column_strs("( ");
        QString value_strs("( ");

        QJsonObject table_schema = mSchema.value(name).toObject();
        QJsonObject columns_schema = table_schema.value("columns").toObject();
        QVariantList params;
        auto query_columns = query_obj.value("columns").toObject();
        auto keys = query_columns.keys();
        for(auto column_name: keys ){
            if(columns_schema.contains(column_name)){
                QJsonObject column_insert = query_columns.value(column_name).toObject();
                QJsonValue value = column_insert.value("value");
                int column_type = column_insert.value("type").toInt(-1);
                column_strs.append(column_name);
                value_strs.append('?');
                if(column_name.compare(keys.last()) == 0){
                    column_strs.append(" ) ");
                    value_strs.append(" ) ");
                }
                else{
                    column_strs.append(", ");
                    value_strs.append(", ");
                }
                params.append(valueToVariant(column_type,value));
            } else {
                return false;
            }
        }
        query.append(column_strs);
        query.append("VALUES");
        query.append(value_strs);

        return sendQueryToDB(query,params);
    }
    bool sendQueryToDB(QString text, QVariantList  values = QVariantList())
    {
        QSqlDatabase db = QSqlDatabase::database(mRootPath+mDatabaseName);
        //db.setHostName("AEMAC");
        //db.setDatabaseName(mRootPath+mDatabaseName);
        //db.setConnectOptions();
        bool ret_val(false);
        if(db.open()){
            QSqlQuery sql_query(db);
            sql_query.prepare(text);
            int vc = values.size();
            for(int index(0); index < vc; ++index)
            {
                sql_query.addBindValue(values.at(index));
            }
            ret_val = sql_query.exec();
            db.commit();
            //db.close();
            //QSqlDatabase::removeDatabase(mRootPath+mDatabaseName);
        }
        return ret_val;
    }
    QSqlQueryModel* fetchTableModel(QString select, QVariantList values = QVariantList())
    {
        QSqlDatabase db = QSqlDatabase::database(mRootPath+mDatabaseName);
//        db.setHostName("AEMAC");
//        db.setDatabaseName(mRootPath+mDatabaseName);
//        db.setConnectOptions();
        QSqlQueryModel* model(nullptr);
        if(db.open()){
            model = new QSqlQueryModel(nullptr);
            model->setQuery(select, db);
            //db.close();
            //QSqlDatabase::removeDatabase(mRootPath+mDatabaseName);
        }
        return model ;
    }
    QVariant boolToVariant(int type, bool b)
    {
        QVariant var;
        switch(type) // what type to convert to
        {
            case QMetaType::Bool:
            {
                return QVariant::fromValue(b);
            }
            case QMetaType::UInt: // fall through
            case QMetaType::ULong: // same thing
            case QMetaType::Int:
            {
                if(b)
                    var = QVariant::fromValue(1);
                else
                    var = QVariant::fromValue(0);

                break;
            }
            case QMetaType::ULongLong:
            case QMetaType::Double:{
                double d(0);
                if(b)
                    d = 1;
                var = QVariant::fromValue(d);
                break;
            }
            case QMetaType::QDateTime:
            case QMetaType::QDate:{
                break;
            }
                // Stored as date or time defaults
                // Stored as Json
            case QMetaType::QJsonArray:
            {
                QJsonArray a;
                a.append(b);
                var = QVariant::fromValue(a);
                break;
            }
            case QMetaType::QJsonObject:
            {
                QJsonObject a;
                a.insert("0",b);
                var = QVariant::fromValue(a);
                break;
            }
            case QMetaType::QJsonValue:
            {
                QJsonValue v(b);
                var = QVariant::fromValue(v);
                break;
            }

            case QMetaType::Char:
            case QMetaType::QString:
            {
                if(b)
                    var = QVariant::fromValue('1');
                else
                    var = QVariant::fromValue('0');
                break;
            }
            case QMetaType::QVariant:{
                var = QVariant::fromValue(b);
                break;
            }
            default:
            {
                var = QVariant::fromValue(b);
                break;
            }
        }
        return var;
    }
    QVariant arrayToVariant(int type, QJsonArray a)
    {
        QVariant var;
        switch(type) // what type to convert to
        {
            case QMetaType::Bool:
            {
                if(a.size() > 0){
                    return QVariant::fromValue(false);
                }else {
                    return QVariant::fromValue(a.first().toBool());
                }
            }
            case QMetaType::ULong:
            case QMetaType::UInt:
            {
                return QVariant::fromValue(static_cast<unsigned int>(a.size()));
            }
            case QMetaType::Int:
            {
                var = QVariant::fromValue(a.size());
                break;
            }
            case QMetaType::ULongLong:
            case QMetaType::Double:{
                double d = a.size();
                var = QVariant::fromValue(d);
                break;
            }
            case QMetaType::QDateTime:
            case QMetaType::QDate:{
                var = QVariant::fromValue(false);
                break;
            }
            case QMetaType::QJsonArray:
            {
                var = QVariant::fromValue(a);
                break;
            }
            case QMetaType::QJsonObject:
            {
                QJsonDocument doc;
                QJsonObject obj;
                for(int index(0); index < a.size(); ++index){
                    obj.insert(QString::number(index),a.at(index));
                }
                doc.setObject(obj);
                var = QVariant::fromValue(doc.toJson(QJsonDocument::Compact));
                break;
            }
            case QMetaType::QJsonValue:
            {
                QJsonValue v(a);
                var = QVariant::fromValue(v);
                break;
            }

            case QMetaType::Char:
            case QMetaType::QString:
            {
                QJsonDocument doc;
                doc.setArray(a);
                var = QVariant::fromValue(doc.toJson(QJsonDocument::Compact));
                break;
            }
            case QMetaType::QVariant:{
                var = QVariant::fromValue(a);
                break;
            }
            default:
            {
                break;
            }
        }
        return var;
    }
    QVariant valueToVariant(int type, QJsonValue value)
    {
        QVariant var;
        int val_type = value.type();
        switch(val_type)
        {
            case QJsonValue::Bool:{
                bool b = value.toBool();
                var = boolToVariant(type,b);
                break;
            }
            case QJsonValue::Array:{
                QJsonDocument doc;
                doc.setArray(value.toArray());
                var = QVariant::fromValue(doc.toJson(QJsonDocument::Compact));
                break;
            }
            case QJsonValue::Object:{
                QJsonDocument doc;
                doc.setObject(value.toObject());
                var = QVariant::fromValue(doc.toJson(QJsonDocument::Compact));
                break;
            }
            case QJsonValue::Double:{
                double d = value.toDouble();
                var = QVariant::fromValue(d);
                break;
            }
            case QJsonValue::String:{
                QString s = value.toString();
                var = QVariant::fromValue(s);
                break;
            }
            default:
            {
                var = QVariant::fromValue(value.toInt());
                break;
            }
        }
        return var;
    }

    QString jvalueToString(QJsonValue value)
    {
        QString ret_val("");
        int type =value.type();
        switch(type){
            case QJsonValue::Null: {
                ret_val = "NULL";
                break;
            }
            case QMetaType::QByteArray:{
                ret_val = value.toString();
                break;
            }
            case QJsonValue::Bool: {
                bool val = value.toBool();
                if(val){
                    ret_val = "1";
                }
                else{
                    ret_val = "0";
                }
                break;
            }
            case QJsonValue::Double: {
                double val = value.toDouble();
                ret_val = QString::number(val);
                break;
            }
            case QJsonValue::String: {
                ret_val = value.toString();
                ret_val.prepend("'");
                ret_val.append("'");
                break;
            }
            case QJsonValue::Array:{
                QJsonDocument doc;
                doc.setArray(value.toArray());
                ret_val = doc.toJson();
                ret_val.prepend("'");
                ret_val.append("'");
                break;
            }
            case QJsonValue::Object:{
                QJsonDocument doc;
                doc.setObject(value.toObject());
                ret_val = doc.toJson();
                ret_val.prepend("'");
                ret_val.append("'");
                break;
            }
            default:{
                break;
            }
        }
        return ret_val;
    }

    QString qtTypeToSqliteType(int type)
    {
        QString str_type("INT");
        switch(type){
            case QMetaType::Int: break;
            case QMetaType::Char: str_type = "TEXT"; break;
            case QMetaType::UInt: str_type = "NUM";break;
            case QMetaType::ULong: str_type = "NUM"; break;
            case QMetaType::ULongLong: str_type = "NUM"; break;
            case QMetaType::QByteArray: str_type = "BLOB"; break;
            case QMetaType::QString: str_type = "TEXT"; break;
            case QMetaType::Float: str_type = "REAL"; break;
            case QMetaType::Double: str_type = "REAL"; break;
            case QMetaType::Bool: break;
                // Stored as date or time defaults
            case QMetaType::QDate: str_type = "NUM"; break;
            case QMetaType::QDateTime: str_type = "NUM"; break;
                // Stored as Json
            case QMetaType::QJsonArray: str_type = "TEXT"; break;
            case QMetaType::QJsonObject: str_type = "TEXT"; break;
            case QMetaType::QJsonValue: str_type = "BLOB"; break;
            default: str_type = "BLOB"; break;
        }
        return str_type;
    }
};


#endif // ASTORAGEPROVIDER_P_H
