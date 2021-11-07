// main provides a build tool for the codebase.
// 
// Building proceeds in the following steps:
//   1. Using dependency information, construct a DAG of targets.
//   2. On a workgroup of goroutines, schedule targets as long as their dependencies are fulfilled.
package main

import (
    "flag"
	"fmt"
    "io"
    "log"
	"os"
    "os/exec"
	"path/filepath"
    "reflect"
	"regexp"
	"runtime"
	"strings"
    "time"
)

var (
    asan = flag.Bool("asan", false, "include asan flags, if available")
    clean = flag.Bool("clean", false, "set to true to always recompile everything")
    verbose = flag.Bool("verbose", false, "whether to output the commands being run")
)

func configure(p *Platform, b *Builder) {
    b.Library(
        "third-party",
        []string{"third-party"},
        p.mustFiles("third-party"),
        nil,
    )
    b.Library(
        "lib",
        []string{"lib"},
        p.mustFiles("lib"),
        []string{"third-party"},
    )

    p.mustBinaries(
        b,
        "bin",
        []string{"lib", "third-party"},
    )
}

func main() {
    flag.Parse()

    p := NewPlatform()
    b := NewBuilder(p)

    b.Options.Asan = *asan
    b.Options.Clean = *clean
    b.Options.Verbose = *verbose

    configure(p, b)

    b.MustBuild()
}

func cflags(additionalIncludes []string) []string{
    switch runtime.GOOS {
    case "windows":
        var flags []string
        for _, include := range additionalIncludes {
            flags = append(flags, "/I" + include)
        }

        return append([]string{
            "/c",
            "/std:c++20",
            "/Z7",
            "/EHsc",
            "/Ithird-party.windows\\",
            "/Ithird-party.windows\\glew-2.1.0\\include",
        }, flags...)
    case "darwin":
        var flags []string
        for _, include := range additionalIncludes {
            flags = append(flags, "-I" + include)
        }

        if *asan {
            flags = append(flags, "-fsanitize=address")
        }

        return append(append([]string{
            "-fcolor-diagnostics",
            "-c",
            "-Werror",
            "-Wno-deprecated",
            "-Wno-missing-braces",
            "-g",
            "-std=c++20",
            "-I/opt/homebrew/opt/llvm/include",
        }, pkgConfig("--cflags", "glew", "glfw3")...), flags...)
    case "linux":
        var flags []string
        for _, include := range additionalIncludes {
            flags = append(flags, "-I" + include)
        }

        return append(append([]string{
            "-fcolor-diagnostics",
            "-c",
            "-Werror",
            "-Wno-deprecated",
            "-Wno-missing-braces",
            "-g",
            "-std=c++20",
        }, pkgConfig("--cflags", "glew", "glfw3")...), flags...)
    default:
        log.Fatal("unsupported platform")
    }

    return nil
}

func ldflags(additionalLibraries []string) []string {
    switch runtime.GOOS {
    case "windows":
        return append([]string{
            `third-party.windows\zlib.lib`,
            `third-party.windows\glfw3_mt.lib`,
            `third-party.windows\glew-2.1.0\lib\Release\x64\glew32s.lib`,
            `user32.lib`,
            `gdi32.lib`,
            `shell32.lib`,
            `opengl32.lib`,
        }, additionalLibraries...)
    case "darwin":
        flags := additionalLibraries

        if *asan {
            flags = append(flags, "-fsanitize=address")
        }

        return append(append([]string{
            "-lz",
            "-framework",
            "OpenGL",
            "-L/opt/homebrew/opt/llvm/lib",
        }, pkgConfig("--libs", "glew", "glfw3")...), flags...)
    case "linux":
        return append(append([]string{
            "-lz",
            "-lGL",
        }, pkgConfig("--libs", "glew", "glfw3")...), additionalLibraries...)
    default:
        log.Fatal("unsupported platform")
    }

    return nil
}

func pkgConfig(kind string, libs ...string) []string {
    libs = append([]string{kind}, libs...)
    cmd := exec.Command("pkg-config", libs...)

    stdoutPipe, err := cmd.StdoutPipe()
    if err != nil {
        log.Fatal(fmt.Sprintf("failed to open stdout for pkg-config: %v", err))
    }
    if err := cmd.Start(); err != nil {
        log.Fatal(fmt.Sprintf("failed to start pkg-config: %v", err))
    }
    stdoutB, err := io.ReadAll(stdoutPipe)
    if err != nil {
        log.Fatal(fmt.Sprintf("failed to read pkg-config stdout: %v", err))
    }
    stdout := strings.TrimSpace(string(stdoutB))
    if err := cmd.Wait(); err != nil {
        log.Fatal(fmt.Sprintf("failed to wait for pkg-config: %v", err))
    }

    return strings.Split(stdout, " ")
}

type SourceFile string

func (f SourceFile) LastUpdated() time.Time {
    info, err := os.Stat(string(f))
    if err != nil {
        log.Fatal(fmt.Sprintf("failed to stat %v", f))
    }

    return info.ModTime()
}

type ObjectFile string

func (f ObjectFile) LastUpdated() time.Time {
    info, err := os.Stat(string(f))
    if err != nil {
        // Assume file does not exist.
        return time.Time{}
    }

    return info.ModTime()
}

var suffixRE = regexp.MustCompile(`\.cc?$`)

func objectFile(s SourceFile) ObjectFile {
	suffix := ".o"
	if runtime.GOOS == "windows" {
		suffix = ".obj"
	}

	return ObjectFile(
		filepath.Join(
			"build",
			suffixRE.ReplaceAllString(
				strings.ReplaceAll(
					string(s),
					string(filepath.Separator),
					"-"),
				suffix),
		),
	)
}

func platformSuffixes() ([]*regexp.Regexp, []*regexp.Regexp) {
	switch runtime.GOOS {
	case "darwin":
		return []*regexp.Regexp{
			regexp.MustCompile(`\.macos\.cc?$`),
			regexp.MustCompile(`\.posix\.cc?$`),
		}, []*regexp.Regexp{
            regexp.MustCompile(`\.linux.cc?$`),
            regexp.MustCompile(`\.windows.cc?$`),
        }
	case "linux":
		return []*regexp.Regexp{
			regexp.MustCompile(`\.linux\.cc?$`),
			regexp.MustCompile(`\.posix\.cc?$`),
		}, []*regexp.Regexp{
            regexp.MustCompile(`\.macos.cc?$`),
            regexp.MustCompile(`\.windows.cc?$`),
        }
	case "windows":
		return []*regexp.Regexp{
			regexp.MustCompile(`\.windows\.cc?$`),
		}, []*regexp.Regexp{
            regexp.MustCompile(`\.macos.cc?$`),
            regexp.MustCompile(`\.linux.cc?$`),
            regexp.MustCompile(`\.posix.cc?$`),
        }
	default:
		log.Fatal("unsupported platform")
	}

    return nil, nil
}

// findFiles recursively finds source files in a directory matching patterns.
func findFiles(dir string, includes []*regexp.Regexp, excludes []*regexp.Regexp) ([]SourceFile, error) {
	entries, err := os.ReadDir(dir)
	if err != nil {
		return nil, fmt.Errorf("failed to read dir %v: %v", dir, err)
	}

	var matches []SourceFile

    L:
	for _, entry := range entries {
		if entry.IsDir() {
			subdir := filepath.Join(dir, entry.Name())

			subMatches, err := findFiles(subdir, includes, excludes)
			if err != nil {
				return nil, fmt.Errorf("failed to find source files in subdir %v: %v", subdir, err)
			}

			matches = append(matches, subMatches...)
			continue
		}

        for _, pattern := range excludes {
            if pattern.MatchString(entry.Name()) {
                continue L
            }
        }

		for _, pattern := range includes {
			if pattern.MatchString(entry.Name()) {
				matches = append(matches, SourceFile(filepath.Join(dir, entry.Name())))
				break
			}
		}
	}

	return matches, nil
}

func compileCmd(source SourceFile, additionalIncludes []string) (*exec.Cmd, ObjectFile) {
    object := objectFile(source)

    if runtime.GOOS == "windows" {
        args := append(cflags(additionalIncludes), []string{
            string(source),
            "/Fo" + string(object),
        }...)

        if *verbose {
            fmt.Printf("$ cl.exe %s\n", strings.Join(args, " "))
        }

        return exec.Command("cl.exe", args...), object
    } else {
        args := append(cflags(additionalIncludes), []string{
            string(source),
            "-o",
            string(object),
        }...)

        if *verbose {
            fmt.Printf("$ %s %s\n", os.Getenv("CXX"), strings.Join(args, " "))
        }

        return exec.Command(os.Getenv("CXX"), args...), object
    }
}

func compile(source SourceFile, additionalIncludes []string) error {
    cmd, _ := compileCmd(source, additionalIncludes)

    stdoutPipe, err := cmd.StdoutPipe()
    if err != nil {
        log.Fatal(fmt.Sprintf("failed to open stdout for compile: %v", err))
    }
    stderrPipe, err := cmd.StderrPipe()
    if err != nil {
        log.Fatal(fmt.Sprintf("failed to open stderr for compile: %v", err))
    }
    if err := cmd.Start(); err != nil {
        log.Fatal(fmt.Sprintf("failed to start compile: %v", err))
    }
    stdoutB, err := io.ReadAll(stdoutPipe)
    if err != nil {
        log.Fatal(fmt.Sprintf("failed to read stdout for compile: %v", err))
    }
    stderrB, err := io.ReadAll(stderrPipe)
    if err != nil {
        log.Fatal(fmt.Sprintf("failed to read stderr for compile: %v", err))
    }
    stdout := string(stdoutB)
    stderr := string(stderrB)
    if err := cmd.Wait(); err != nil {
        if _, ok := err.(*exec.ExitError); ok {
            fmt.Println(stdout)
            fmt.Println(stderr)

            return fmt.Errorf("compile failed")
        } else {
            log.Fatal(fmt.Sprintf("failed to wait for compile: %v", err))
        }
    }

    return nil
}

func linkLibrary(name string, objects []ObjectFile) (string, bool) {
    var cmd *exec.Cmd

    var filename string

    if runtime.GOOS == "windows" {
        filename = filepath.Join("build", "lib" + name + ".lib")

        args := []string{
            "/OUT:" + filename,
        }
        for _, object := range objects {
            args = append(args, string(object))
        }

        cmd = exec.Command("lib.exe", args...)
    } else {
        filename = filepath.Join("build", "lib" + name + ".a")

        args := []string{
            "-r",
            filename,
        }
        for _, object := range objects {
            args = append(args, string(object))
        }

        cmd = exec.Command("ar", args...)
    }

    if *verbose {
        fmt.Printf("$ %s\n", cmd)
    }

    stdoutPipe, err := cmd.StdoutPipe()
    if err != nil {
        log.Fatal(fmt.Sprintf("failed to open stdout for link: %v", err))
    }
    stderrPipe, err := cmd.StderrPipe()
    if err != nil {
        log.Fatal(fmt.Sprintf("failed to open stderr for link: %v", err))
    }
    if err := cmd.Start(); err != nil {
        log.Fatal(fmt.Sprintf("failed to start link: %v", err))
    }
    stdoutB, err := io.ReadAll(stdoutPipe)
    if err != nil {
        log.Fatal(fmt.Sprintf("failed to read stdout for link: %v", err))
    }
    stderrB, err := io.ReadAll(stderrPipe)
    if err != nil {
        log.Fatal(fmt.Sprintf("failed to read stderr for link: %v", err))
    }
    stdout := string(stdoutB)
    stderr := string(stderrB)
    if err := cmd.Wait(); err != nil {
        if _, ok := err.(*exec.ExitError); ok {
            fmt.Println(stdout)
            fmt.Println(stderr)

            return filename, true
        } else {
            log.Fatal(fmt.Sprintf("failed to wait for link: %v", err))
        }
    }

    return filename, false
}

func linkBinary(name string, objects []ObjectFile, libs []string) (string, bool) {
    var cmd *exec.Cmd

    var filename string

    if runtime.GOOS == "windows" {
        filename = filepath.Join("build", name + ".exe")

        args := append([]string{
            "/Fe" + filename,
            "/Z7",
            "/link",
            "/NODEFAULTLIB:MSVCRT",
        }, ldflags(libs)...)

        for _, object := range objects {
            args = append([]string{string(object)}, args...)
        }

        cmd = exec.Command("cl.exe", args...)
    } else {
        filename = filepath.Join("build", name)

        args := append([]string{
            "-o",
            filename,
        }, ldflags(libs)...)
        for _, object := range objects {
            args = append([]string{string(object)}, args...)
        }

        cmd = exec.Command(os.Getenv("CXX"), args...)
    }

    if *verbose {
        fmt.Printf("$ %s\n", cmd)
    }

    stdoutPipe, err := cmd.StdoutPipe()
    if err != nil {
        log.Fatal(fmt.Sprintf("failed to open stdout for link: %v", err))
    }
    stderrPipe, err := cmd.StderrPipe()
    if err != nil {
        log.Fatal(fmt.Sprintf("failed to open stderr for link: %v", err))
    }
    if err := cmd.Start(); err != nil {
        log.Fatal(fmt.Sprintf("failed to start link: %v", err))
    }
    stdoutB, err := io.ReadAll(stdoutPipe)
    if err != nil {
        log.Fatal(fmt.Sprintf("failed to read stdout for link: %v", err))
    }
    stderrB, err := io.ReadAll(stderrPipe)
    if err != nil {
        log.Fatal(fmt.Sprintf("failed to read stderr for link: %v", err))
    }
    stdout := string(stdoutB)
    stderr := string(stderrB)
    if err := cmd.Wait(); err != nil {
        if _, ok := err.(*exec.ExitError); ok {
            fmt.Println(stdout)
            fmt.Println(stderr)

            return filename, true
        } else {
            log.Fatal(fmt.Sprintf("failed to wait for link: %v", err))
        }
    }

    return filename, false
}

type Platform struct {
    goos string
    excludes []*regexp.Regexp
    includes []*regexp.Regexp
}

func NewPlatform() *Platform {
    includes, excludes := platformSuffixes()
    includes = append(includes, regexp.MustCompile(`\.cc?$`))

    return &Platform{
        goos: runtime.GOOS,
        excludes: excludes,
        includes: includes,
    }
}

func (p *Platform) exclude(n string) bool {
    for _, exclude := range p.excludes {
        if exclude.MatchString(n) {
            return true
        }
    }

    return false
}

func (p *Platform) include(n string) bool {
    for _, include := range p.includes {
        if include.MatchString(n) {
            return true
        }
    }

    return false
}

func (p *Platform) mustFiles(dir string) []SourceFile {
    files, err := p.files(dir)
    if err != nil {
        log.Fatal(err)
    }

    return files
}

func (p *Platform) files(dir string) ([]SourceFile, error) {
	entries, err := os.ReadDir(dir)
	if err != nil {
		return nil, fmt.Errorf("failed to read dir %v: %v", dir, err)
	}

	var matches []SourceFile

	for _, entry := range entries {
		if entry.IsDir() {
			subdir := filepath.Join(dir, entry.Name())

			subMatches, err := p.files(subdir)
			if err != nil {
				return nil, fmt.Errorf("failed to find source files in subdir %v: %v", subdir, err)
			}

			matches = append(matches, subMatches...)
			continue
		}

        if p.exclude(entry.Name()) {
            continue
        }

        if !p.include(entry.Name()) {
            continue
        }

        matches = append(matches, SourceFile(filepath.Join(dir, entry.Name())))
	}

	return matches, nil
}

func (p *Platform) mustBinaries(b *Builder, location string, deps []string) {
    if err := p.binaries(b, location, deps); err != nil {
        log.Fatal(err)
    }
}

func (p *Platform) libraryName(name string) string {
    switch p.goos {
    case "windows":
        return filepath.Join("build", "lib"+name+".lib")
    default:
        return filepath.Join("build", "lib"+name+".a")
    }
}

func (p *Platform) binaries(b *Builder, location string, deps []string) error {
    entries, err := os.ReadDir(location)
    if err != nil {
        return fmt.Errorf("failed to read binary directory: %w", err)
    }

    for _, entry := range entries {
        var name string
        var dir *string
        var files []SourceFile

        if entry.IsDir() {
            var err error

            s := filepath.Join("bin", entry.Name())

            name = entry.Name()
            dir = &s
            files, err = p.files(*dir)
            if err != nil {
                return fmt.Errorf("failed to find files for binary %q: %w", *dir, err)
            }
        } else {
            if p.exclude(entry.Name()) {
                continue
            }

            if !p.include(entry.Name()) {
                continue
            }

            name = strings.TrimSuffix(entry.Name(), filepath.Ext(entry.Name()))
            files = []SourceFile{
                SourceFile(filepath.Join(location, entry.Name())),
            }
        }

        b.Binary(name, files, dir, deps)
    }

    return nil
}

type Library struct {
    Name string
    Includes []string
}

type Binary struct {
}

type TaskImpl interface {
    Do(*Builder, *Task) error
}

type Compile struct {
    source SourceFile
    includes []string
}

func (c *Compile) Do(b *Builder, t *Task) error {
    object := objectFile(c.source)

    if !b.Options.Clean && c.source.LastUpdated().Before(object.LastUpdated()) {
        if b.Options.Verbose {
            fmt.Printf("[v] %q up to date\n", t.Name)
        }
        return nil
    }

    if err := compile(c.source, c.includes); err != nil {
        return fmt.Errorf("%q compile failed: %v", t.Name, err)
    }

    return nil
}

type LinkLibrary struct {
    objects []ObjectFile
    name string
}

func (l *LinkLibrary) Do(b *Builder, t *Task) error {
    _, failed := linkLibrary(l.name, l.objects)
    if failed {
        return fmt.Errorf("%q link failed", t.Name)
    }

    return nil
}

type LinkBinary struct {
    objects []ObjectFile
    libs []string
    name string
}

func (l *LinkBinary) Do(b *Builder, t *Task) error {
    _, failed := linkBinary(l.name, l.objects, l.libs)
    if failed {
        return fmt.Errorf("%q link failed", t.Name)
    }

    return nil
}

type Nop struct {
}

func (*Nop) Do(b *Builder, t *Task) error {
    return nil
}

type Continue int

const (
    ContinueYes Continue = iota
    ContinueNo
)

type Task struct {
    builder *Builder
    Name string
    Parents []<-chan bool
    Children []chan<- bool

    Impl TaskImpl
}

func (t *Task) Wait() Continue {
    if t.builder.Options.Verbose {
        fmt.Printf("[v] task %q waiting for %d parents\n", t.Name, len(t.Parents))
    }
    var cases []reflect.SelectCase

    for _, parent := range t.Parents {
        cases = append(cases, reflect.SelectCase{
            Dir: reflect.SelectRecv,
            Chan: reflect.ValueOf(parent),
        })
    }

    ret := ContinueYes

    for len(cases) > 0 {
        chosen, value, _ := reflect.Select(cases)
        if !value.Interface().(bool) {
            ret = ContinueNo
        }

        cases = append(cases[:chosen], cases[chosen+1:]...)
    }

    return ret
}

func (t *Task) Do() error {
    if t.Impl == nil {
        log.Fatal(fmt.Sprintf("task %q has no implementation", t.Name))
    }

    return t.Impl.Do(t.builder, t)
}

func (t *Task) Done(succeeded bool) {
    for _, child := range t.Children {
        child <- succeeded
    }
}

type Builder struct {
    platform *Platform

    Options struct {
        Asan bool
        Clean bool
        Verbose bool
    }

    targets map[string]interface{}
    tasks []*Task
    tailTasks map[string]*Task
}

func NewBuilder(platform *Platform) *Builder {
    return &Builder{
        platform: platform,
        targets: make(map[string]interface{}),
        tailTasks: make(map[string]*Task),
    }
}

func (b *Builder) Library(name string, includeDirs []string, files []SourceFile, deps []string) *Builder {
    b.targets[name] = &Library{
        Name: b.platform.libraryName(name),
        Includes: includeDirs,
    }

    head := &Task{
        builder: b,
        Name: fmt.Sprintf("[lib] %s:head", name),
        Impl: &Nop{},
    }
    b.tasks = append(b.tasks, head)

    var parentIncludes []string

    for _, dep := range deps {
        tail, ok := b.tailTasks[dep]
        if !ok {
            log.Fatal(fmt.Sprintf("target %q: failed to find dependency %q", name, dep))
        }

        c := make(chan bool)
        tail.Children = append(tail.Children, c)
        head.Parents = append(head.Parents, c)

        parentLib, ok := b.targets[dep].(*Library)
        if !ok {
            log.Fatal(fmt.Sprintf("target %q: dependency %q is not a library", name, dep))
        }

        parentIncludes = append(parentIncludes, parentLib.Includes...)
    }

    tail := &Task{
        builder: b,
        Name: fmt.Sprintf("[lib] ==> %s:link", name),
    }
    b.tasks = append(b.tasks, tail)

    includes := append(includeDirs, parentIncludes...)

    var objects []ObjectFile
    for _, file := range files {
        objects = append(objects, objectFile(file))

        compile := &Task{
            builder: b,
            Name: fmt.Sprintf("[lib] %s:%s", name, string(file)),
            Impl: &Compile{
                source: file,
                includes: includes,
            },
        }

        c := make(chan bool)
        head.Children = append(head.Children, c)
        compile.Parents = append(compile.Parents, c)

        d := make(chan bool)
        tail.Parents = append(tail.Parents, d)
        compile.Children = append(compile.Children, d)

        b.tasks = append(b.tasks, compile)
    }

    tail.Impl = &LinkLibrary{
        objects: objects,
        name: name,
    }

    b.tailTasks[name] = tail
    return b
}

func (b *Builder) Binary(name string, files []SourceFile, dir *string, deps []string) *Builder {
    b.targets[name] = &Binary{
    }

    head := &Task{
        builder: b,
        Name: fmt.Sprintf("[bin] %s:head", name),
        Impl: &Nop{},
    }
    b.tasks = append(b.tasks, head)

    var parentIncludes []string
    var libs []string

    for _, dep := range deps {
        tail, ok := b.tailTasks[dep]
        if !ok {
            log.Fatal(fmt.Sprintf("target %q: failed to find dependency %q", name, dep))
        }

        c := make(chan bool)
        tail.Children = append(tail.Children, c)
        head.Parents = append(head.Parents, c)

        parentLib, ok := b.targets[dep].(*Library)
        if !ok {
            log.Fatal(fmt.Sprintf("target %q: dependency %q is not a library", name, dep))
        }

        libs = append(libs, parentLib.Name)

        parentIncludes = append(parentIncludes, parentLib.Includes...)
    }

    tail := &Task{
        builder: b,
        Name: fmt.Sprintf("[bin] ==> %s:link", name),
    }
    b.tasks = append(b.tasks, tail)

    if dir != nil {
        parentIncludes = append(parentIncludes, *dir)
    }

    var objects []ObjectFile
    for _, file := range files {
        objects = append(objects, objectFile(file))

        compile := &Task{
            builder: b,
            Name: fmt.Sprintf("[bin] %s:%s", name, string(file)),
            Impl: &Compile{
                source: file,
                includes: parentIncludes,
            },
        }

        c := make(chan bool)
        head.Children = append(head.Children, c)
        compile.Parents = append(compile.Parents, c)

        d := make(chan bool)
        tail.Parents = append(tail.Parents, d)
        compile.Children = append(compile.Children, d)

        b.tasks = append(b.tasks, compile)
    }

    tail.Impl = &LinkBinary{
        objects: objects,
        libs: libs,
        name: name,
    }

    b.tailTasks[name] = tail
    return b
}

func (b *Builder) MustBuild() {
    if err := b.Build(); err != nil {
        log.Fatal(err)
    }
}

func (b *Builder) Build() error {
    workQueue := make(chan *Task, runtime.NumCPU())
    defer close(workQueue)

    done := make(chan bool, len(b.tasks))
    errors := make(chan error, len(b.tasks))

    for i := 0; i < runtime.NumCPU(); i++ {
        go b.worker(workQueue, done, errors)
    }

    for _, task := range b.tasks {
        if b.Options.Verbose {
            fmt.Printf("[v] task %q\n", task.Name)
        }
    }

    for _, task := range b.tasks {
        go func(t *Task) {
            if t.Wait() == ContinueNo {
                t.Done(false)
                done <- true
                return
            }

            workQueue <- t
        }(task)
    }

    var allErrors []error
    doneCount := 0
    for doneCount < len(b.tasks) {
        select {
        case <-done:
            doneCount++
        case err := <- errors:
            allErrors = append(allErrors, err)
        }
    }

    if len(allErrors) > 0 {
        // TODO: stuff the errors in here?
        return fmt.Errorf("build failed with errors")
    }

    return nil
}

func (b *Builder) worker(workQueue <-chan *Task, done chan bool, errors chan error) {
    // TODO: For now, we just print the target name.
    for task := range workQueue {
        fmt.Printf("%s\n", task.Name)

        err := task.Do()
        if err != nil {
            fmt.Printf("task failed: %v\n", err)
            errors <- err
        }

        task.Done(err == nil)
        done <- true
    }
}
