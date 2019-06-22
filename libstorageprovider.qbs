Product{
    name:"libstorageprovider"
    type:"staticlibrary"
    Depends{ name:"cpp"}
    cpp.cxxLanguageVersion: "c++11"
    Depends{name:  "Qt"; submodules:["core",  "core-private", "sql"]}
    cpp.defines:['ASTRGE_CORE_LIBRARY']
    qbs.install:true
    Group{
        name:"source"
        files:["astorageprovider.cpp"]
    }
    Group{
        name:"private"
        files:[
            "private/asql_statement_p.h",
            "private/astorageprovider_p.h",
            "private/astorageprovider_lib.h",
        ]
    }
    Group{
        name:"headers"
        qbs.installDir: "include/"
        qbs.install:  true
        files:["astorageprovider.h"]
    }
}
