{{- /* ------------------------------------------------------------------ */ -}}
{{-                         define "EnumCase"                                -}}
{{- /* Prints the 'Enum::kEntry' name for the provided  sem.EnumEntry     */ -}}
{{- /* argument.                                                          */ -}}
{{- /* ------------------------------------------------------------------ */ -}}
{{PascalCase $.Enum.Name}}::k{{PascalCase $.Name}}
{{- end -}}

{{- /* ------------------------------------------------------------------ */ -}}
{{-                         define "DeclareEnum"                             -}}
{{- /* Declares the 'enum class' for the provided sem.Enum argument.      */ -}}
{{- /* ------------------------------------------------------------------ */ -}}
{{- $enum    := PascalCase $.Name -}}
enum class {{$enum}} {
    kInvalid,
{{-   range $entry := $.Entries }}
    k{{PascalCase $entry.Name}},{{if $entry.IsInternal}}  // Tint-internal enum entry - not parsed{{end}}
{{-   end }}
};

/// @param out the std::ostream to write to
/// @param value the {{$enum}}
/// @returns `out` so calls can be chained
std::ostream& operator<<(std::ostream& out, {{$enum}} value);

/// Parse{{$enum}} parses a {{$enum}} from a string.
/// @param str the string to parse
/// @returns the parsed enum, or {{$enum}}::kInvalid if the string could not be parsed.
{{$enum}} Parse{{$enum}}(std::string_view str);

{{- end -}}


{{- /* ------------------------------------------------------------------ */ -}}
{{-                          define "ParseEnum"                              -}}
{{- /* Implements the 'ParseEnum' function for the provided sem.Enum      */ -}}
{{- /* argument.                                                          */ -}}
{{- /* ------------------------------------------------------------------ */ -}}
{{- $enum    := PascalCase $.Name -}}
/// Parse{{$enum}} parses a {{$enum}} from a string.
/// @param str the string to parse
/// @returns the parsed enum, or {{$enum}}::kInvalid if the string could not be parsed.
{{$enum}} Parse{{$enum}}(std::string_view str) {
{{- $tree := MinimalPerfectHash $.PublicNameSet }}
    uint64_t u64 = 0u;
{{ Eval "HashTree" "enum" $enum "tree" $tree }}
    return {{$enum}}::kInvalid;
}
{{- end -}}


{{- /* ------------------------------------------------------------------ */ -}}
{{-                          define "HashTree"                               -}}
{{- /* ------------------------------------------------------------------ */ -}}
{{- $tree    := $.tree   -}}
{{- $enum    := $.enum   -}}
{{/* indent */}}    {{/*    */ -}}
    memcpy(&u64, str.data(), std::min(str.size(), sizeof(u64)));
    switch (((u64 * {{$tree.Multiplier}}) % {{$tree.Prime}}) % {{len $tree.Bins}}) {
{{-   range $i, $bin := $tree.Bins }}
        case {{$i}}:
{{-     if $bin.Children }}
            if (u64 == 0x{{printf "%.8x" $bin.U64}}) {
                str = str.substr(8);  // Skip the 8 bytes of the processed u64
                u64 = 0;              // Clear the u64 for the memcpy
{{              Eval "HashTree" "enum" $enum "tree" $bin.Children }}
            }
            break;
{{-     else if le (len $bin.Remaining) 4 }}
            return (u64 == {{$bin.U64}}) ? {{$enum}}::k{{PascalCase $bin.String}} : {{$enum}}::kInvalid;
{{-     else }}
            return (str == "{{$bin.Remaining}}") ? {{$enum}}::k{{PascalCase $bin.String}} : {{$enum}}::kInvalid;
{{-     end }}
{{-   end }}
    }
{{- end -}}

{{- /* ------------------------------------------------------------------ */ -}}
{{-                         define "EnumOStream"                             -}}
{{- /* Implements the std::ostream 'operator<<()' function to print the   */ -}}
{{- /* provided sem.Enum.                                                 */ -}}
{{- /* ------------------------------------------------------------------ */ -}}
{{- $enum    := PascalCase $.Name -}}
std::ostream& operator<<(std::ostream& out, {{$enum}} value) {
    switch (value) {
        case {{$enum}}::kInvalid:
            return out << "invalid";
{{-   range $entry := $.Entries }}
        case {{template "EnumCase" $entry}}:
            return out << "{{$entry.Name}}";
{{-   end }}
    }
    return out << "<unknown>";
}
{{- end -}}


{{- /* ------------------------------------------------------------------ */ -}}
{{-                        define "TestParsePrintEnum"                       -}}
{{- /* Implements unit tests for parsing and printing the provided        */ -}}
{{- /* sem.Enum argument.                                                 */ -}}
{{- /* ------------------------------------------------------------------ */ -}}
{{- $enum    := PascalCase $.Name -}}
namespace parse_print_tests {

struct Case {
    const char* string;
    {{$enum}} value;
};

inline std::ostream& operator<<(std::ostream& out, Case c) {
    return out << "'" << std::string(c.string) << "'";
}

static constexpr Case kValidCases[] = {
{{-   range $entry := $.PublicEntries }}
    {"{{$entry.Name}}", {{template "EnumCase" $entry}}},
{{-   end }}
};

static constexpr Case kInvalidCases[] = {
{{-   $exclude := $.NameSet -}}
{{-   range $entry := $.PublicEntries }}
    {"{{Scramble $entry.Name $exclude}}", {{$enum}}::kInvalid},
    {"{{Scramble $entry.Name $exclude}}", {{$enum}}::kInvalid},
    {"{{Scramble $entry.Name $exclude}}", {{$enum}}::kInvalid},
{{-   end }}
};

using {{$enum}}ParseTest = testing::TestWithParam<Case>;

TEST_P({{$enum}}ParseTest, Parse) {
    const char* string = GetParam().string;
    {{$enum}} expect = GetParam().value;
    EXPECT_EQ(expect, Parse{{$enum}}(string));
}

INSTANTIATE_TEST_SUITE_P(ValidCases, {{$enum}}ParseTest, testing::ValuesIn(kValidCases));
INSTANTIATE_TEST_SUITE_P(InvalidCases, {{$enum}}ParseTest, testing::ValuesIn(kInvalidCases));

using {{$enum}}PrintTest = testing::TestWithParam<Case>;

TEST_P({{$enum}}PrintTest, Print) {
    {{$enum}} value = GetParam().value;
    const char* expect = GetParam().string;
    EXPECT_EQ(expect, utils::ToString(value));
}

INSTANTIATE_TEST_SUITE_P(ValidCases, {{$enum}}PrintTest, testing::ValuesIn(kValidCases));

}  // namespace parse_print_tests

{{- end -}}


{{- /* ------------------------------------------------------------------ */ -}}
{{-                     define "BenchmarkParseEnum"                          -}}
{{- /* Implements a micro-benchmark for parsing the provided sem.Enum     */ -}}
{{- /* argument.                                                          */ -}}
{{- /* ------------------------------------------------------------------ */ -}}
{{- $enum    := PascalCase $.Name -}}
void {{$enum}}Parser(::benchmark::State& state) {
    std::array kStrings{
{{-   $exclude := $.NameSet -}}
{{-   range $entry := $.PublicEntries }}
        "{{Scramble $entry.Name $exclude}}",
        "{{Scramble $entry.Name $exclude}}",
        "{{Scramble $entry.Name $exclude}}",
        "{{$entry.Name}}",
        "{{Scramble $entry.Name $exclude}}",
        "{{Scramble $entry.Name $exclude}}",
        "{{Scramble $entry.Name $exclude}}",
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
