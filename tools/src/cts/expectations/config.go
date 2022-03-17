package expectations

type Config struct {
	Path       string // Relative to dawn root
	TestPrefix string
	Tags       Tags
}

type Tags struct {
	OS                []string
	Devices           []string
	Platform          []string
	Browser           []string
	GPU               []string
	Decoder           []string
	ANGLEBackend      []string
	SkiaRenderer      []string
	SwiftShader       []string
	Driver            []string
	ASan              []string
	DisplayServer     []string
	OOPCanvas         []string
	BackendValidation []string

	// Map of expectation tag to list of expanded tags
	Aliases map[string][]string
}
