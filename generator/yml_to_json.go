package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"math"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"slices"
	"strconv"
	"strings"

	"github.com/goccy/go-yaml"
	"github.com/wI2L/jsondiff"
	"github.com/webgpu-native/webgpu-headers/gen"
)

type StringListFlag []string

var StringListFlagI flag.Value = &StringListFlag{}

func (f *StringListFlag) String() string     { return fmt.Sprintf("%#v", f) }
func (f *StringListFlag) Set(v string) error { *f = append(*f, v); return nil }

var (
	schemaPath       string
	yamlPaths        StringListFlag
	diffDawnJsonPath string
)

type JsonObj map[string]interface{}

func AsDawnName(n string) string {
	return strings.Join(strings.Split(n, "_"), " ")
}

func AsDawnTypeName(n string, pointer gen.PointerType) string {
	l := strings.Split(n, ".")
	n = AsDawnName(l[len(l)-1])
	if n == "uint32" {
		n = "uint32_t"
	} else if n == "uint64" {
		n = "uint64_t"
	} else if n == "usize" {
		n = "size_t"
	} else if n == "float64" {
		n = "double"
	} else if n == "float32" {
		n = "float"
	} else if n == "c void" {
		if pointer == gen.PointerTypeImmutable {
			n = "void const *"
		} else if pointer == gen.PointerTypeMutable {
			n = "void *"
		}
	}
	return n
}

func ParseEnumValue(i int, value string) uint64 {
	if value == "" {
		return uint64(i)
	} else {
		var num string
		var base int
		if strings.HasPrefix(value, "0x") {
			base = 16
			num = strings.TrimPrefix(value, "0x")
		} else {
			base = 10
			num = value
		}
		n, err := strconv.ParseUint(num, base, 64)
		if err != nil {
			panic(err)
		}
		return n
	}
}

func ParseBitflag(i int, value string) uint16 {
	if value == "" {
		value := uint64(math.Pow(2, float64(i-1)))
		return uint16(value)
	} else {
		var num string
		var base int
		if strings.HasPrefix(value, "0x") {
			base = 16
			num = strings.TrimPrefix(value, "0x")
		} else {
			base = 10
			num = value
		}
		value, err := strconv.ParseUint(num, base, 64)
		if err != nil {
			panic(err)
		}
		return uint16(value)
	}
}

func ParseParameters(fargs []gen.ParameterType) []JsonObj {
	var args []JsonObj
	for _, a := range fargs {
		arg := JsonObj{}

		if a.Optional {
			arg["optional"] = true
		}
		if a.Pointer != "" {
			if a.Pointer == gen.PointerTypeImmutable {
				arg["annotation"] = "const*"
			} else if a.Pointer == gen.PointerTypeMutable {
				arg["annotation"] = "*"
			}
		}

		arg["name"] = AsDawnName(a.Name)

		matches := arrayTypeRegexp.FindStringSubmatch(a.Type)
		if len(matches) == 2 {
			length := AsDawnName(gen.Singularize(a.Name)) + " count"
			args = append(args, JsonObj{
				"name": length,
				"type": "size_t",
			})
			arg["type"] = AsDawnTypeName(matches[1], a.Pointer)
			arg["length"] = length
		} else {
			arg["type"] = AsDawnTypeName(a.Type, a.Pointer)
		}

		if arg["type"] == "string" {
			arg["type"] = "char"
			arg["annotation"] = "const*"
			arg["length"] = "strlen"
		} else if arg["type"] == "c void" || arg["type"] == "void *" || arg["type"] == "void const *" {
			if a.Pointer == gen.PointerTypeImmutable {
				arg["type"] = "void const *"
				delete(arg, "annotation")
			} else if a.Pointer == gen.PointerTypeMutable {
				arg["type"] = "void *"
				delete(arg, "annotation")
			}
		}
		args = append(args, arg)
	}
	return args
}

func ParseFunction(f gen.Function) JsonObj {
	json := JsonObj{
		"name": AsDawnName(f.Name),
	}
	if f.Returns != nil {
		json["returns"] = AsDawnTypeName(f.Returns.Type, f.Returns.Pointer)
	}
	args := ParseParameters(f.Args)
	if f.ReturnsAsync != nil {
		args = append(args, JsonObj{
			"name": "callback",
			"type": AsDawnName(f.Name) + " callback",
		})
		args = append(args, JsonObj{
			"name": "userdata",
			"type": "void *",
		})
	}
	if args != nil {
		json["args"] = args
	}
	return json
}

func AddTag(o *JsonObj, tag string) {
	if tag == "webgpu" {
		return
	}
	if (*o)["tags"] == nil {
		(*o)["tags"] = []string{}
	}
	(*o)["tags"] = append((*o)["tags"].([]string), strings.Split(tag, "_")...)
}

func AddTags(os []JsonObj, tag string) {
	if tag == "webgpu" {
		return
	}
	for _, o := range os {
		AddTag(&o, tag)
	}
}

var arrayTypeRegexp = regexp.MustCompile(`array<([a-zA-Z0-9._]+)>`)

var EmscriptenNoEnumTable = []string{
	"request adapter status",
	"adapter type",
	"backend type",
	"buffer map async status",
	"compilation message type",
	"create pipeline async status",
	"device lost reason",
	"pop error scope status",
	"error type",
	"callback mode",
	"wait status",
	"present mode",
	"queue work done status",
	"request device status",
	"s type",
}

func main() {
	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, `yml_to_json.go:
  Parses the webgpu.yml file and converts it to dawn.json format. Prints the output.
  Pass -diff-dawn-json to compare to an existing dawn.json file and print that instead.

Arguments:
`)
		flag.PrintDefaults()
	}
	flag.StringVar(&schemaPath, "schema", "", "path of the json schema")
	flag.Var(&yamlPaths, "yaml", "path of the yaml spec")
	flag.StringVar(&diffDawnJsonPath, "diff-dawn-json", "", "path to dawn.json for comparison")
	flag.Parse()
	if schemaPath == "" || len(yamlPaths) == 0 {
		flag.Usage()
		os.Exit(1)
	}

	// Order matters for validation steps, so enforce it.
	if len(yamlPaths) > 1 && filepath.Base(yamlPaths[0]) != "webgpu.yml" {
		panic(`"webgpu.yml" must be the first sequence in the order`)
	}

	// Validate the yaml files (jsonschema, duplications)
	if err := gen.ValidateYamls(schemaPath, yamlPaths); err != nil {
		panic(err)
	}

	jsonOut := JsonObj{}
	for _, yamlPath := range yamlPaths {
		src, err := os.ReadFile(yamlPath)
		if err != nil {
			panic(err)
		}

		var yml gen.Yml
		if err := yaml.Unmarshal(src, &yml); err != nil {
			panic(err)
		}

		enumPrefix := ParseEnumValue(0, yml.EnumPrefix+"0000")

		for _, c := range yml.Constants {
			value := c.Value
			if value == "usize_max" {
				value = "size_max"
			}

			var typ string
			if strings.Contains(value, "uint32") {
				typ = "uint32_t"
			} else if strings.Contains(value, "uint64") {
				typ = "uint64_t"
			} else if strings.Contains(value, "size") {
				typ = "size_t"
			} else {
				panic("Unsupported type for value:" + value)
			}

			jsonOut[AsDawnName(c.Name)] = JsonObj{
				"category": "constant",
				"value":    strings.ToUpper(value),
				"type":     typ,
			}
		}

		for _, e := range yml.Enums {
			var values []JsonObj

			for i, entry := range e.Entries {
				v := enumPrefix + ParseEnumValue(i, entry.Value)
				json := JsonObj{
					"value": v,
					"name":  AsDawnName(entry.Name),
				}
				if e.Name == "WGSL_feature_name" {
					json["jsrepr"] = "'" + entry.Name + "'"
				}
				if json["name"] == "undefined" {
					json["jsrepr"] = "undefined"
				}
				values = append(values, json)
			}

			if e.Extended {
				json := jsonOut[AsDawnName(e.Name)].(JsonObj)
				AddTags(values, yml.Name)
				json["values"] = append(json["values"].([]JsonObj), values...)
				jsonOut[AsDawnName(e.Name)] = json
			} else {
				json := JsonObj{
					"category": "enum",
					"values":   values,
				}
				if slices.Contains(EmscriptenNoEnumTable, AsDawnName(e.Name)) {
					json["emscripten_no_enum_table"] = true
				}

				jsonOut[AsDawnName(e.Name)] = json
			}
		}

		for _, b := range yml.Bitflags {
			var values []JsonObj

			for i, entry := range b.Entries {
				var jsonValue uint16
				if len(entry.ValueCombination) > 0 {
					jsonValue = 0
					for _, v := range entry.ValueCombination {
						idx := slices.IndexFunc(b.Entries, func(e gen.BitflagEntry) bool {
							return e.Name == v
						})
						jsonValue += ParseBitflag(idx, b.Entries[idx].Value)
					}
				} else {
					jsonValue = ParseBitflag(i, entry.Value)
				}

				values = append(values, JsonObj{
					"value": jsonValue,
					"name":  AsDawnName(entry.Name),
				})
			}

			jsonOut[AsDawnName(b.Name)] = JsonObj{
				"category": "bitmask",
				"values":   values,
			}
		}

		// FunctionTypes
		for _, f := range yml.FunctionTypes {
			json := ParseFunction(f)
			json["category"] = "function pointer"
			AddTag(&json, yml.Name)
			jsonOut[AsDawnName(f.Name)] = json
		}

		// Functions
		for _, f := range yml.Functions {
			json := ParseFunction(f)
			json["category"] = "function"
			AddTag(&json, yml.Name)
			jsonOut[AsDawnName(f.Name)] = json
		}

		AddAsyncCallbacks := func(name string, params []gen.ParameterType) {
			name = AsDawnName(name) + " callback"
			// Trim off "get" from the callback name
			if strings.HasPrefix(name, "get ") {
				name = strings.TrimPrefix(name, "get ")
			}
			args := ParseParameters(params)
			json := JsonObj{
				"category": "function pointer",
			}
			if args != nil {
				json["args"] = args
			}
			jsonOut[name] = json
		}

		// Add function pointers for callbacks
		for _, f := range yml.Functions {
			if f.ReturnsAsync == nil {
				continue
			}
			AddAsyncCallbacks(f.Name, f.ReturnsAsync)
		}
		for _, f := range yml.FunctionTypes {
			if f.ReturnsAsync == nil {
				continue
			}
			AddAsyncCallbacks(f.Name, f.ReturnsAsync)
		}
		for _, o := range yml.Objects {
			for _, f := range o.Methods {
				if f.ReturnsAsync == nil {
					continue
				}
				AddAsyncCallbacks(f.Name, f.ReturnsAsync)
			}
		}

		// Structs
		for _, s := range yml.Structs {
			members := ParseParameters(s.Members)
			json := JsonObj{
				"category": "structure",
			}
			if members != nil {
				json["members"] = members
			}
			switch s.Type {
			case "standalone":
				json["extensible"] = false
				break
			case "base_in":
				json["extensible"] = "in"
				break
			case "base_out":
				json["extensible"] = "out"
				break
			case "extension_in":
				json["chained"] = "in"
				break
			case "extension_out":
				json["chained"] = "out"
				break
			}
			AddTag(&json, yml.Name)
			jsonOut[AsDawnName(s.Name)] = json
		}

		// Objects
		for _, o := range yml.Objects {
			var methods []JsonObj
			for _, m := range o.Methods {
				methods = append(methods, ParseFunction(m))
			}

			if o.Extended {
				AddTags(methods, yml.Name)
				json := jsonOut[AsDawnName(o.Name)].(JsonObj)
				json["methods"] = append(json["methods"].([]JsonObj), methods...)
				jsonOut[AsDawnName(o.Name)] = json
			} else {
				json := JsonObj{
					"category": "object",
					"methods":  methods,
				}
				AddTag(&json, yml.Name)
				jsonOut[AsDawnName(o.Name)] = json
			}
		}
	}

	// Add in Dawn-internal things.
	jsonOut["ObjectHandle"] = JsonObj{
		"_comment": "Only used for the wire",
		"category": "native",
	}
	jsonOut["ObjectId"] = JsonObj{
		"_comment": "Only used for the wire",
		"category": "native",
	}
	jsonOut["ObjectType"] = JsonObj{
		"_comment": "Only used for the wire",
		"category": "native",
	}
	jsonOut["bool"] = JsonObj{
		"category": "native",
	}
	jsonOut["uint16_t"] = JsonObj{
		"category": "native",
	}
	jsonOut["uint32_t"] = JsonObj{
		"category": "native",
	}
	jsonOut["uint64_t"] = JsonObj{
		"category": "native",
	}
	jsonOut["uint8_t"] = JsonObj{
		"category": "native",
	}
	jsonOut["void"] = JsonObj{
		"category": "native",
	}
	jsonOut["void *"] = JsonObj{
		"category": "native",
	}
	jsonOut["void const *"] = JsonObj{
		"category": "native",
	}

	generatedJson, err := json.MarshalIndent(jsonOut, "", "  ")
	if err != nil {
		panic(err)
	}

	if diffDawnJsonPath != "" {
		// Read dawn.json
		dawnJsonSrc, err := os.ReadFile(diffDawnJsonPath)
		if err != nil {
			panic(err)
		}

		// Unmarshal and marshal to normalize
		dawnJson := JsonObj{}
		err = json.Unmarshal(dawnJsonSrc, &dawnJson)
		if err != nil {
			panic(err)
		}
		delete(dawnJson, "_comment")
		delete(dawnJson, "_metadata")
		delete(dawnJson, "_doc")
		dawnJsonSrc, err = json.MarshalIndent(dawnJson, "", "  ")
		if err != nil {
			panic(err)
		}

		patch, err := jsondiff.Compare(jsonOut, dawnJson)
		if err != nil {
			panic(err)
		}
		var removed []jsondiff.Operation
		var added []jsondiff.Operation
		var replaced []jsondiff.Operation
		for _, op := range patch {
			if op.Type == "remove" {
				removed = append(removed, op)
			} else if op.Type == "add" {
				added = append(added, op)
			} else if op.Type == "replace" {
				replaced = append(replaced, op)
			} else {
				panic("Unsupported op type: " + op.Type)
			}
		}
		fmt.Printf("Removed:\n")
		for _, op := range removed {
			fmt.Printf("  %v\n", op)
		}
		fmt.Printf("Added:\n")
		for _, op := range added {
			fmt.Printf("  %v\n", op)
		}
		fmt.Printf("Replaced:\n")
		for _, op := range replaced {
			fmt.Printf("  %v\n", op)
		}

		f1, err := os.CreateTemp("", "f1")
		if err != nil {
			panic(err)
		}
		defer os.Remove(f1.Name())
		f2, err := os.CreateTemp("", "f2")
		if err != nil {
			panic(err)
		}
		defer os.Remove(f2.Name())

		_, err = f1.Write(dawnJsonSrc)
		if err != nil {
			panic(err)
		}
		_, err = f2.Write(generatedJson)
		if err != nil {
			panic(err)
		}

		cmd := exec.Command("git", "diff", "--no-index", "--minimal", f1.Name(), f2.Name())
		output, _ := cmd.Output()
		os.Stdout.Write(output)
	} else {
		fmt.Println(string(generatedJson))
	}
}
