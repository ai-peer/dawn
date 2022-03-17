package expectations

import "fmt"

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

	nameToType tagNameToType
}

type TagType string

const (
	tagOS                TagType = "OS"
	tagDevices           TagType = "Devices"
	tagPlatform          TagType = "Platform"
	tagBrowser           TagType = "Browser"
	tagGPU               TagType = "GPU"
	tagDecoder           TagType = "Decoder"
	tagANGLEBackend      TagType = "ANGLE Backend"
	tagSkiaRenderer      TagType = "Skia renderer"
	tagSwiftShader       TagType = "SwiftShader"
	tagDriver            TagType = "Driver"
	tagASan              TagType = "ASan"
	tagDisplayServer     TagType = "Display server"
	tagOOPCanvas         TagType = "OOP-Canvas"
	tagBackendValidation TagType = "Backend validation"
)

type tagNameToType map[string]TagType

func (c *Config) TagNameToType() (tagNameToType, error) {
	if c.Tags.nameToType != nil {
		return c.Tags.nameToType, nil
	}

	tags := tagNameToType{}
	for _, t := range []struct {
		ty   TagType
		tags []string
	}{
		{tagOS, c.Tags.OS},
		{tagDevices, c.Tags.Devices},
		{tagPlatform, c.Tags.Platform},
		{tagBrowser, c.Tags.Browser},
		{tagGPU, c.Tags.GPU},
		{tagDecoder, c.Tags.Decoder},
		{tagANGLEBackend, c.Tags.ANGLEBackend},
		{tagSkiaRenderer, c.Tags.SkiaRenderer},
		{tagSwiftShader, c.Tags.SwiftShader},
		{tagDriver, c.Tags.Driver},
		{tagASan, c.Tags.ASan},
		{tagDisplayServer, c.Tags.DisplayServer},
		{tagOOPCanvas, c.Tags.OOPCanvas},
		{tagBackendValidation, c.Tags.BackendValidation},
	} {
		for _, tag := range t.tags {
			if _, collision := tags[tag]; collision {
				return nil, fmt.Errorf("Duplicate tag definition '%v'", tag)
			}
			tags[tag] = t.ty
		}
	}
	c.Tags.nameToType = tags
	return tags, nil
}

func (c *Config) Validate() error {
	tagNameToType, err := c.TagNameToType()
	if err != nil {
		return err
	}
	for alias, tags := range c.Tags.Aliases {
		for _, tag := range tags {
			if _, found := tagNameToType[tag]; !found {
				return fmt.Errorf(`Tags.Alias '%v' has unknown tag '%v'`, alias, tag)
			}
		}
	}
	return nil
}
