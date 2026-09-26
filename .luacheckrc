-- Keep this allowlist aligned with the supported authoring surface in
-- LuaNpcScripts::Implementation. The global table and chunk loaders are not authoring APIs.
std = {
    read_globals = {
        "_VERSION",
        "assert",
        "collectgarbage",
        "error",
        "getmetatable",
        "ipairs",
        "math",
        "next",
        "pairs",
        "pcall",
        "print",
        "rawequal",
        "rawget",
        "rawlen",
        "rawset",
        "select",
        "setmetatable",
        "string",
        "table",
        "tonumber",
        "tostring",
        "type",
        "warn",
        "xpcall",
    },
}

codes = true
max_line_length = 100
ignore = { "212/self" }
