#ifndef ASQL_STATEMENT_P_H
#define ASQL_STATEMENT_P_H
#include <QObject>
#include <QSharedData>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>
class ASqlCreateTable;
class ASqlCreateColumn;
// Define the private implicitly shared data
class APrivateSqlStatement : public QSharedData
{
public:
    QJsonObject mQuery;
};

class ASTRGE_LIBRARY_EXPORT ASqlStatement // Internal interface/wrapper on "shared" QJsonObject
{

public:

    enum Query{ CREATE_TABLE = 0,ALTER_TABLE, DROP_TABLE, INSERT_TABLE, SELECT_TABLE};
    friend class ASqlBase;
    friend class APrivateSqliteStorageProvider;
    ASqlStatement(const ASqlStatement& rhs)
        :data(rhs.data){}
    ASqlStatement& operator =(const ASqlStatement & rhs)
    {
        data = rhs.data;
        return (*this);
    }
    ~ASqlStatement()
    {
        data.reset();
    }
    // the explicitly shared internal data
    void setTableName(QString name)
    {
        data->mQuery.insert("name",name);
    }
    void setTableName(QJsonObject select)
    {
        data->mQuery.insert("name",select);
    }
    void setTemporaryTable(bool temporary)
    {
        if(temporary){
            data->mQuery.insert("temp",true);
        }
        else
        {
            data->mQuery.remove("temp");
        }
    }
    void addColumn(QString name, unsigned int type)
    {
        if(name.length() == 0){
            return;
        }
        QJsonObject columns(data->mQuery.value("columns").toObject());
        if(columns.contains(name)){ // update the type
            QJsonObject column = columns.value(name).toObject();
            column.insert("type",int(type));
            columns.insert(name,column);
        }
        else{
            QJsonObject column;
            column.insert("type",int(type));
            columns.insert(name,column);
        }
        data->mQuery.insert(QStringLiteral("columns"), columns);
    }
    void addDistinct(bool distinct)
    {
        if(distinct){
            data->mQuery.insert(QStringLiteral("distinct"), true);
        }
        else{
            data->mQuery.remove(QStringLiteral("distinct"));
        }
    }
    void addWhere(QString where)
    {
        if(data->mQuery.contains(QStringLiteral("where")))
        {
            data->mQuery.remove(QStringLiteral("where"));
        }
        data->mQuery.insert(QStringLiteral("where"),where);
    }
    void setOverrideTable(bool override)
    {
        if(override){
            data->mQuery.insert("override", true);
        }
        else{
            data->mQuery.remove("override");
        }
    }
    void setColumnValue( QString column_name, QString value_name, QJsonValue value)
    {
        // fetch columns
        auto columns(data->mQuery.value("columns").toObject());
        auto column(columns.value(column_name).toObject());
        if(column.isEmpty())
        {
            return;
        }
        if(column.contains(value_name)){
            column.remove(value_name);
        }
        column.insert(value_name,value);
        columns.remove(column_name);
        columns.insert(column_name,column);
        data->mQuery.insert("columns", columns);
    }
    void setQuery(int type)
    {
        data->mQuery.insert("type",type);
    }
    const QJsonObject& query() const {return data->mQuery;}
protected:
    QExplicitlySharedDataPointer<APrivateSqlStatement> data;
    ASqlStatement():data(new APrivateSqlStatement){}
};

class ASTRGE_LIBRARY_EXPORT ASqlBase
{

public:
    ASqlBase(){}
    ~ASqlBase(){

    }
    ASqlBase(const ASqlBase& rhs)
        :statement(rhs.statement){}
    const ASqlStatement  end() const;

protected:
    ASqlStatement statement;
};

class ASqlSelectColumn;
class ASTRGE_LIBRARY_EXPORT ASqlSelectWhere : public ASqlBase
{

public:
    friend class ASqlSelect;
    friend class ASqlSelectColumn;
    ASqlSelectWhere where(QString where);
protected:
    ASqlSelectWhere() : ASqlBase(){}
    ASqlSelectWhere(const ASqlBase& rhs):ASqlBase(rhs){}
};

class ASTRGE_LIBRARY_EXPORT ASqlSelectColumn : public ASqlBase
{
public:
    friend class ASqlSelect;
    ASqlSelectColumn addColumnFilter(QString column);
    ASqlSelectWhere   where(QString where_clause);
    ASqlSelectColumn min(QString column);
    ASqlSelectColumn max(QString column);
protected:
    ASqlSelectColumn() : ASqlBase() {}
    ASqlSelectColumn(const ASqlBase& rhs):ASqlBase(rhs){}
};


class ASTRGE_LIBRARY_EXPORT ASqlSelect : public ASqlBase
{
public:
    ASqlSelect(QString table = "") : ASqlBase()
    {
        statement.setTableName(table);
        statement.setQuery(ASqlStatement::SELECT_TABLE);
    }

    ASqlSelect(ASqlSelect& inner_statement) : ASqlBase(inner_statement)
    {
        statement.setTableName(inner_statement.statement.query());
        statement.setQuery(ASqlStatement::SELECT_TABLE);
    }

    ASqlSelect(const ASqlBase& rhs, QString name = "")
        : ASqlBase(rhs)
    {
        if(!name.isEmpty()){
            statement.setTableName(name);
        }
        statement.setQuery(ASqlStatement::SELECT_TABLE);
    }
    ASqlSelect                 setDistinct(bool distinct = true);
    ASqlSelectColumn addColumnFilter(QString column);
    ASqlSelectColumn min(QString column);
    ASqlSelectColumn max( QString column);
    ASqlSelectWhere   where(QString where_clause);
};

class ASTRGE_LIBRARY_EXPORT ASqlCreateColumn : public ASqlBase
{
public:
    friend class ASqlCreateTable;
    ASqlCreateColumn setDefault(QJsonValue value);
    ASqlCreateColumn setUnique(bool unique = true);
    ASqlCreateColumn setPrimaryKey(bool primary = true);
    ASqlCreateColumn setAutoIncrement(bool increment = true);
    ASqlCreateColumn setNotNull(bool not_null = true);
    ASqlCreateColumn addColumn(QString name, unsigned int type);
    ASqlCreateColumn addCheck(QString check);
protected:
    QString name;
    unsigned int type;
    ASqlCreateColumn() : ASqlBase(){}
    ASqlCreateColumn(const ASqlBase& rhs):ASqlBase(rhs){}
};

class ASTRGE_LIBRARY_EXPORT ASqlCreateTable : public ASqlBase  // interface for creating a table
{
public:
    ASqlCreateTable(QString name = "", bool temporary = false) : ASqlBase()
    {
        statement.setTableName(name);
        statement.setTemporaryTable(temporary);
        statement.setQuery(ASqlStatement::CREATE_TABLE);
    }
    ASqlCreateTable setTemporary(bool temporary = true);
    ASqlCreateTable setOverride(bool override = true);
    ASqlCreateTable setName(QString name);
    ASqlCreateColumn addColumn(QString name, unsigned int type);
protected:
    ASqlCreateTable(const ASqlBase& rhs):ASqlBase(rhs){}
};

class ASqlInsert;

class ASTRGE_LIBRARY_EXPORT ASqlInsertValue :  public ASqlBase
{
public:
    friend class ASqlInsert;
    ASqlInsertValue addValue(QString name, QJsonValue value);
    ASqlInsertValue  addValue(QString name, QByteArray blob);
    ASqlInsertValue  addValue(QString name, QVariant  value);
protected:
    ASqlInsertValue(): ASqlBase(){}
    ASqlInsertValue(const ASqlBase& rhs): ASqlBase(rhs){}
};

class ASTRGE_LIBRARY_EXPORT ASqlInsert : public ASqlBase
{
public:
    ASqlInsert(QString name = "")
        : ASqlBase()
    {
        statement.setTableName(name);
        statement.setQuery(ASqlStatement::INSERT_TABLE);
    }
    ASqlInsert(const ASqlBase& rhs, QString name = "")
        : ASqlBase(rhs)
    {
        if(!name.isEmpty()){
            statement.setTableName(name);
        }
        statement.setQuery(ASqlStatement::INSERT_TABLE);
    }
    ASqlInsertValue  addValue(QString name, QJsonValue value);
    ASqlInsertValue  addValue(QString name, QByteArray blob);
    ASqlInsertValue  addValue(QString name, QVariant  value);
};

#endif // ASQL_STATEMENT_P_H
