//go:build windows

package main

import (
	"bufio"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
	"sync"
	"syscall"
)

const createNoWindow = 0x08000000

func appDirectory() string {
	executable, err := os.Executable()
	if err != nil {
		return "."
	}
	return filepath.Dir(executable)
}

func defaultFirmwareFolder() string {
	base := appDirectory()
	for _, candidate := range []string{
		filepath.Join(base, "Firmware", "BIN"),
		filepath.Join(base, "BIN"),
		base,
	} {
		if info, err := os.Stat(candidate); err == nil && info.IsDir() {
			return candidate
		}
	}
	return base
}

func locateEsptool() (string, error) {
	besideApp := filepath.Join(appDirectory(), "esptool.exe")
	if info, err := os.Stat(besideApp); err == nil && !info.IsDir() {
		return besideApp, nil
	}
	if found, err := exec.LookPath("esptool.exe"); err == nil {
		return found, nil
	}
	return "", errors.New("esptool.exe is missing. Keep Espressif's esptool.exe in the same folder as Clock Builder")
}

var comPattern = regexp.MustCompile(`(?i)\bCOM\d+\b`)

func discoverPorts() []string {
	command := exec.Command("reg.exe", "query", `HKLM\HARDWARE\DEVICEMAP\SERIALCOMM`)
	command.SysProcAttr = &syscall.SysProcAttr{CreationFlags: createNoWindow, HideWindow: true}
	output, err := command.CombinedOutput()
	if err != nil {
		return nil
	}
	seen := make(map[string]bool)
	ports := make([]string, 0, 4)
	for _, match := range comPattern.FindAllString(string(output), -1) {
		port := strings.ToUpper(match)
		if !seen[port] {
			seen[port] = true
			ports = append(ports, port)
		}
	}
	sort.Slice(ports, func(i, j int) bool {
		var left, right int
		fmt.Sscanf(ports[i], "COM%d", &left)
		fmt.Sscanf(ports[j], "COM%d", &right)
		return left < right
	})
	return ports
}

type commandRunner struct {
	mu  sync.Mutex
	cmd *exec.Cmd
}

func (r *commandRunner) running() bool {
	r.mu.Lock()
	defer r.mu.Unlock()
	return r.cmd != nil && r.cmd.Process != nil
}

func (r *commandRunner) start(executable string, arguments []string,
	onOutput func(string), onFinish func(int)) error {
	r.mu.Lock()
	if r.cmd != nil {
		r.mu.Unlock()
		return errors.New("another operation is already running")
	}
	command := exec.Command(executable, arguments...)
	command.SysProcAttr = &syscall.SysProcAttr{CreationFlags: createNoWindow, HideWindow: true}
	stdout, err := command.StdoutPipe()
	if err != nil {
		r.mu.Unlock()
		return err
	}
	stderr, err := command.StderrPipe()
	if err != nil {
		r.mu.Unlock()
		return err
	}
	if err := command.Start(); err != nil {
		r.mu.Unlock()
		return err
	}
	r.cmd = command
	r.mu.Unlock()

	go func() {
		var readers sync.WaitGroup
		consume := func(reader io.Reader) {
			defer readers.Done()
			scanner := bufio.NewScanner(reader)
			buffer := make([]byte, 4096)
			scanner.Buffer(buffer, 1024*1024)
			for scanner.Scan() {
				onOutput(scanner.Text() + "\n")
			}
		}
		readers.Add(2)
		go consume(stdout)
		go consume(stderr)
		err := command.Wait()
		readers.Wait()
		exitCode := 0
		if err != nil {
			if failure, ok := err.(*exec.ExitError); ok {
				exitCode = failure.ExitCode()
			} else {
				exitCode = -1
			}
		}
		r.mu.Lock()
		r.cmd = nil
		r.mu.Unlock()
		onFinish(exitCode)
	}()
	return nil
}

func (r *commandRunner) cancel() {
	r.mu.Lock()
	defer r.mu.Unlock()
	if r.cmd != nil && r.cmd.Process != nil {
		_ = r.cmd.Process.Kill()
	}
}

func esptoolIdentifyArguments(port string) []string {
	return []string{"--port", port, "--baud", "115200", "flash-id"}
}

func esptoolEraseArguments(port string, baud int) []string {
	return []string{
		"--chip", "esp32", "--port", port, "--baud", fmt.Sprint(baud),
		"--before", "default-reset", "--after", "hard-reset", "erase-flash",
	}
}

func esptoolWriteArguments(port string, baud int, imagePath string) []string {
	return []string{
		"--chip", "esp32", "--port", port, "--baud", fmt.Sprint(baud),
		"--before", "default-reset", "--after", "hard-reset",
		"write-flash", "-z", "0x0", imagePath,
	}
}
