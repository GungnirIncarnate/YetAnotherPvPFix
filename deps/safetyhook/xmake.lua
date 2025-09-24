package("safetyhook")
    set_urls("https://github.com/cursey/safetyhook.git")

    add_deps("zydis", "zycore", "cmake")

    add_versions("v0.6.5", "8a975dee560c424329dcb0e151f3649fa4b19146")

    on_install(function (package)
        local configs = {}
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
        import("package.tools.cmake").install(package, configs, { packagedeps = { "zycore", "zydis" } })
        os.cp("include", package:installdir())
        
    end)
package_end()