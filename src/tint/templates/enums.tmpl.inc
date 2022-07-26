{{- /* ------------------------------------------------------------------ */ -}}
{{-                         define "DeclareEnum"                             -}}
{{- /* ------------------------------------------------------------------ */ -}}
{{- $enum    := PascalCase $.Name -}}
enum class {{$enum}} {
    kInvalid,
{{-   range $entry := $.Entries }}
    k{{PascalCase $entry.Name}},
{{-   end }}
};

/// @param out the std::ostream to write to
/// @param value the {{$enum}}
/// @return the std::ostream so calls can be chained
std::ostream& operator<<(std::ostream& out, {{$enum}} value);

/// Parse{{$enum}} parses a {{$enum}} from a string.
/// @param str the string to parse
/// @returns the parsed enum, or {{$enum}}::kInvalid if the string could not be parsed.
{{$enum}} Parse{{$enum}}(std::string_view str);

{{- end -}}


{{- /* ------------------------------------------------------------------ */ -}}
{{-                          define "ParseEnum"                              -}}
{{- /* ------------------------------------------------------------------ */ -}}
{{- $enum    := PascalCase $.Name -}}
/// Parse{{$enum}} parses a {{$enum}} from a string.
/// @param str the string to parse
/// @returns the parsed enum, or {{$enum}}::kInvalid if the string could not be parsed.
{{$enum}} Parse{{$enum}}(std::string_view str) {
{{-   range $entry := $.PublicEntries }}
    if (str == "{{$entry.Name}}") {
        return {{$enum}}::k{{PascalCase $entry.Name}};
    }
{{-   end }}
    return {{$enum}}::kInvalid;
}
{{- end -}}


{{- /* ------------------------------------------------------------------ */ -}}
{{-                         define "EnumOStream"                             -}}
{{- /* ------------------------------------------------------------------ */ -}}
{{- $enum    := PascalCase $.Name -}}
std::ostream& operator<<(std::ostream& out, {{$enum}} value) {
    switch (value) {
        case {{$enum}}::kInvalid:
            return out << "invalid";
{{-   range $entry := $.Entries }}
        case {{$enum}}::k{{PascalCase $entry.Name}}:
            return out << "{{$entry.Name}}";
{{-   end }}
    }
    return out << "<unknown>";
}
{{- end -}}


{{- /* ------------------------------------------------------------------ */ -}}
{{-                     define "BenchmarkParseEnum"                          -}}
{{- /* ------------------------------------------------------------------ */ -}}
{{- $enum    := PascalCase $.Name -}}
void {{$enum}}Parser(::benchmark::State& state) {
    std::array kStrings{
{{-   range $entry := $.PublicEntries }}
        "{{Scramble $entry.Name}}",
        "{{Scramble $entry.Name}}",
        "{{Scramble $entry.Name}}",
        "{{$entry.Name}}",
        "{{Scramble $entry.Name}}",
        "{{Scramble $entry.Name}}",
        "{{Scramble $entry.Name}}",
{{-   end }}
    };
    for (auto _ : state) {
        for (auto& str : kStrings) {
            auto result = Parse{{$enum}}(str);
            benchmark::DoNotOptimize(result);
        }
    }
}

BENCHMARK({{$enum}}Parser);
{{- end -}}
