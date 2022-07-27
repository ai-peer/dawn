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
    kUndefined,
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
/// @returns the parsed enum, or {{$enum}}::kUndefined if the string could not be parsed.
{{$enum}} Parse{{$enum}}(std::string_view str);

constexpr const char* k{{$enum}}Strings[] = {
{{-   range $entry := $.Entries }}
{{-     if not $entry.IsInternal}}
    "{{$entry.Name}}",
{{-     end }}
{{-   end }}
};

{{- end -}}


{{- /* ------------------------------------------------------------------ */ -}}
{{-                          define "ParseEnum"                              -}}
{{- /* Implements the 'ParseEnum' function for the provided sem.Enum      */ -}}
{{- /* argument.                                                          */ -}}
{{- /* ------------------------------------------------------------------ */ -}}
{{- $enum    := PascalCase $.Name -}}
/// Parse{{$enum}} parses a {{$enum}} from a string.
/// @param str the string to parse
/// @returns the parsed enum, or {{$enum}}::kUndefined if the string could not be parsed.
{{$enum}} Parse{{$enum}}(std::string_view str) {
{{- $node := MinimalPerfectHash $.PublicNameSet }}
    std::array<uint64_t, 5> u64s = {};
    memcpy(u64s.data(), str.data(), str.size());
{{ Eval "HashTreeNode" "enum" $enum "node" $node "word" 0 }}
    return {{$enum}}::kUndefined;
}
{{- end -}}


{{- /* ------------------------------------------------------------------ */ -}}
{{-                          define "HashTreeNode"                           -}}
{{- /* ------------------------------------------------------------------ */ -}}
{{- $node    := $.node   -}}
{{- $enum    := $.enum   -}}
{{- $word    := $.word   -}}
{{/* indent */}}    {{/*    */ -}}
{{- if lt (len $node.Bins) 4 }}
{{-   range $i, $bin := $node.Bins }}
{{      Eval "HashTreeBin" "enum" $enum "bin" $bin "word" $word }}
{{-   end }}
{{- else }}
      switch (((u64s[{{$word}}] * {{$node.Multiplier}}) % {{$node.Prime}}) & {{$node.Mask}}) {
{{-     range $i, $bin := $node.Bins }}
        case {{$i}}: {
{{    Eval "HashTreeBin" "enum" $enum "bin" $bin "word" $word }}
            return {{$enum}}::kUndefined;
        }
{{-     end }}
      }
{{-   end }}
{{- end -}}


{{- /* ------------------------------------------------------------------ */ -}}
{{-                          define "HashTreeBin"                           -}}
{{- /* ------------------------------------------------------------------ */ -}}
{{- $bin  := $.bin     -}}
{{- $enum := $.enum   -}}
{{- $word := $.word   -}}
{{/* indent */}}    {{/*    */ -}}
    if (u64s[{{$word}}] == {{printf "0x%.16x" $bin.U64}}) {
{{- if $bin.Children }}
{{-   $next_word := Add $word 1 }}
{{    Eval "HashTreeNode" "enum" $enum "node" $bin.Children "word" $next_word}}
{{- else }}
      return {{$enum}}::k{{PascalCase $bin.String}};
{{- end }}
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
        case {{$enum}}::kUndefined:
            return out << "undefined";
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

static constexpr Case kUndefinedCases[] = {
{{-   $exclude := $.NameSet -}}
{{-   range $entry := $.PublicEntries }}
    {"{{Scramble $entry.Name $exclude}}", {{$enum}}::kUndefined},
    {"{{Scramble $entry.Name $exclude}}", {{$enum}}::kUndefined},
    {"{{Scramble $entry.Name $exclude}}", {{$enum}}::kUndefined},
{{-   end }}
};

using {{$enum}}ParseTest = testing::TestWithParam<Case>;

TEST_P({{$enum}}ParseTest, Parse) {
    const char* string = GetParam().string;
    {{$enum}} expect = GetParam().value;
    EXPECT_EQ(expect, Parse{{$enum}}(string));
}

INSTANTIATE_TEST_SUITE_P(ValidCases, {{$enum}}ParseTest, testing::ValuesIn(kValidCases));
INSTANTIATE_TEST_SUITE_P(InvalidCases, {{$enum}}ParseTest, testing::ValuesIn(kUndefinedCases));

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
