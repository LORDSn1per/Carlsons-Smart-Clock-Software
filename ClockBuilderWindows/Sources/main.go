//go:build windows

package main

import (
	"fmt"
	"os/exec"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"unsafe"
)

var appVersion = "dev"
var application *clockBuilder
var windowCallback = syscall.NewCallback(windowProcedure)

const (
	idPortCombo  = 1001
	idImageCombo = 1002
	idBaudCombo  = 1003
	idLogEdit    = 1004
)

type operationStage int

const (
	stageIdle operationStage = iota
	stageIdentifying
	stageFlashing
	stageDone
	stageFailed
)

type appFonts struct {
	tiny, small, body, bodyBold, heading, hero, display uintptr
}

type clockBuilder struct {
	mu sync.Mutex

	window    uintptr
	instance  uintptr
	fonts     appFonts
	bgBrush   uintptr
	wellBrush uintptr

	portCombo, imageCombo, baudCombo, logEdit uintptr
	buttons                                   map[string]rect
	hovered, pressed                          string

	ports           []string
	selectedPort    int
	folder          string
	images          []firmwareImage
	selectedImage   int
	fileMode        bool
	baud            int
	eraseEverything bool
	showLog         bool

	device          *deviceInfo
	deviceProblem   string
	stage           operationStage
	progress        float64
	statusMessage   string
	log             string
	busy            bool
	cancelRequested bool
	runner          commandRunner
}

var (
	colorBG        = rgb(8, 10, 15)
	colorPanel     = rgb(16, 20, 27)
	colorRaised    = rgb(21, 26, 35)
	colorWell      = rgb(11, 14, 20)
	colorLine      = rgb(39, 44, 54)
	colorStrong    = rgb(62, 69, 82)
	colorMuted     = rgb(141, 150, 168)
	colorText      = rgb(244, 246, 248)
	colorAccent    = rgb(217, 255, 98)
	colorAccentInk = rgb(20, 26, 8)
	colorCyan      = rgb(47, 217, 243)
	colorDanger    = rgb(255, 107, 114)
	colorGood      = rgb(142, 231, 168)
	colorWarning   = rgb(255, 181, 71)
)

func main() {
	procSetProcessDPI.Call()
	instance, _, _ := procGetModuleHandle.Call(0)
	app := &clockBuilder{
		instance:      instance,
		buttons:       make(map[string]rect),
		selectedPort:  -1,
		selectedImage: -1,
		folder:        defaultFirmwareFolder(),
		baud:          460800,
		stage:         stageIdle,
	}
	application = app

	className := utf16Ptr("CarlsonClockBuilderWindow")
	cursor, _, _ := procLoadCursor.Call(0, 32512)
	class := wndClassEx{
		Size:      uint32(unsafe.Sizeof(wndClassEx{})),
		Style:     0x0002 | 0x0001,
		WndProc:   windowCallback,
		Instance:  instance,
		Cursor:    cursor,
		ClassName: className,
	}
	if registered, _, _ := procRegisterClassEx.Call(uintptr(unsafe.Pointer(&class))); registered == 0 {
		messageBox(0, "Clock Builder", "Windows could not register the application window.", mbOK|mbIconError)
		return
	}

	title := fmt.Sprintf("Clock Builder %s", appVersion)
	window, _, _ := procCreateWindowEx.Call(
		0,
		uintptr(unsafe.Pointer(className)),
		uintptr(unsafe.Pointer(utf16Ptr(title))),
		wsOverlappedWindow|wsClipChildren,
		uintptr(uint32(0x80000000)), uintptr(uint32(0x80000000)),
		1050, 900, 0, 0, instance, 0,
	)
	if window == 0 {
		messageBox(0, "Clock Builder", "Windows could not create the application window.", mbOK|mbIconError)
		return
	}
	app.window = window
	setDarkWindow(window)
	procShowWindow.Call(window, swShow)
	procUpdateWindow.Call(window)

	var message msg
	for {
		result, _, _ := procGetMessage.Call(uintptr(unsafe.Pointer(&message)), 0, 0, 0)
		if int32(result) <= 0 {
			break
		}
		procTranslateMessage.Call(uintptr(unsafe.Pointer(&message)))
		procDispatchMessage.Call(uintptr(unsafe.Pointer(&message)))
	}
}

func windowProcedure(window uintptr, message uint32, wParam, lParam uintptr) uintptr {
	if application == nil {
		result, _, _ := procDefWindowProc.Call(window, uintptr(message), wParam, lParam)
		return result
	}
	app := application
	switch message {
	case wmCreate:
		app.window = window
		app.createControls()
		app.refreshPorts()
		app.refreshImages()
		return 0
	case wmPaint:
		app.paint()
		return 0
	case wmSize:
		procInvalidateRect.Call(window, 0, 0)
		return 0
	case wmCommand:
		app.command(int(loWord(wParam)), int(hiWord(wParam)))
		return 0
	case wmMouseMove:
		app.mouseMove(xFromLParam(lParam), yFromLParam(lParam))
		return 0
	case wmLButtonDown:
		app.mouseDown(xFromLParam(lParam), yFromLParam(lParam))
		return 0
	case wmLButtonUp:
		app.mouseUp(xFromLParam(lParam), yFromLParam(lParam))
		return 0
	case wmAppRefresh:
		app.refreshFromWorker()
		return 0
	case wmCtlColorEdit, wmCtlColorListBox:
		procSetTextColor.Call(wParam, colorText)
		procSetBkColor.Call(wParam, colorWell)
		return app.wellBrush
	case wmClose:
		app.runner.cancel()
	case wmDestroy:
		app.destroyResources()
		procPostQuitMessage.Call(0)
		return 0
	}
	result, _, _ := procDefWindowProc.Call(window, uintptr(message), wParam, lParam)
	return result
}

func (app *clockBuilder) createControls() {
	app.fonts = appFonts{
		tiny:     createFont(-11, 700, "Segoe UI"),
		small:    createFont(-13, 500, "Segoe UI"),
		body:     createFont(-15, 400, "Segoe UI"),
		bodyBold: createFont(-15, 650, "Segoe UI"),
		heading:  createFont(-18, 650, "Segoe UI"),
		hero:     createFont(-30, 650, "Segoe UI"),
		display:  createFont(-20, 700, "Consolas"),
	}
	app.bgBrush = createBrush(colorBG)
	app.wellBrush = createBrush(colorWell)
	comboStyle := uint32(wsChild | wsVisible | wsTabStop | wsVScroll | cbsDropdownList | cbsHasStrings)
	app.portCombo = createChild("COMBOBOX", "", comboStyle, idPortCombo, app.window, app.instance)
	app.imageCombo = createChild("COMBOBOX", "", comboStyle, idImageCombo, app.window, app.instance)
	app.baudCombo = createChild("COMBOBOX", "", comboStyle, idBaudCombo, app.window, app.instance)
	app.logEdit = createChild("EDIT", "", wsChild|wsVisible|wsBorder|esMultiline|esReadOnly|esAutoVScroll|wsVScroll,
		idLogEdit, app.window, app.instance)
	for _, control := range []uintptr{app.portCombo, app.imageCombo, app.baudCombo, app.logEdit} {
		procSendMessage.Call(control, wmSetFont, app.fonts.body, 1)
		setDarkWindow(control)
	}
	comboReset(app.baudCombo, []string{"460800", "230400", "115200"}, 0)
	moveWindow(app.logEdit, rect{}, false)
}

func (app *clockBuilder) destroyResources() {
	for _, font := range []uintptr{app.fonts.tiny, app.fonts.small, app.fonts.body, app.fonts.bodyBold, app.fonts.heading, app.fonts.hero, app.fonts.display} {
		if font != 0 {
			procDeleteObject.Call(font)
		}
	}
	if app.bgBrush != 0 {
		procDeleteObject.Call(app.bgBrush)
	}
	if app.wellBrush != 0 {
		procDeleteObject.Call(app.wellBrush)
	}
}

func (app *clockBuilder) paint() {
	var ps paintStruct
	hdc, _, _ := procBeginPaint.Call(app.window, uintptr(unsafe.Pointer(&ps)))
	defer procEndPaint.Call(app.window, uintptr(unsafe.Pointer(&ps)))
	var client rect
	procGetClientRect.Call(app.window, uintptr(unsafe.Pointer(&client)))
	width := client.Right
	height := client.Bottom
	fillRectangle(hdc, client, app.bgBrush)

	for x := int32(0); x < width; x += 32 {
		line(hdc, x, 0, x, height, rgb(14, 18, 25), 1)
	}
	for y := int32(0); y < height; y += 32 {
		line(hdc, 0, y, width, y, rgb(14, 18, 25), 1)
	}
	line(hdc, 0, 60, width, 60, colorLine, 1)

	app.mu.Lock()
	defer app.mu.Unlock()
	app.buttons = make(map[string]rect)
	app.drawHeader(hdc, width)
	app.drawHero(hdc, width)
	app.drawDeviceCard(hdc, width)
	app.drawFirmwareCard(hdc, width)
	app.drawWriteCard(hdc, width)
	app.drawLog(hdc, width)
	app.drawFooter(hdc, width, height)
	app.positionControls(width)
}

func (app *clockBuilder) drawHeader(hdc uintptr, width int32) {
	logo := rect{24, 11, 64, 51}
	outlineRoundedBox(hdc, rect{22, 9, 66, 53}, 12, rgb(68, 82, 38), 4)
	roundedBox(hdc, logo, 10, colorRaised, rgb(81, 96, 43), 1)
	for index := int32(0); index < 4; index++ {
		x := int32(34) + index*5
		top := int32(36) - index*5
		barColor := colorCyan
		if index == 3 {
			barColor = colorAccent
		}
		roundedBox(hdc, rect{x, top, x + 3, 42}, 2, barColor, barColor, 1)
	}
	drawLabel(hdc, "CARLSON SMART CLOCK", rect{78, 12, 300, 28}, app.fonts.tiny, colorMuted, dtLeft|dtSingleLine|dtVCenter)
	drawLabel(hdc, "Clock Builder", rect{78, 25, 250, 52}, app.fonts.heading, colorText, dtLeft|dtSingleLine|dtVCenter)
	roundedBox(hdc, rect{248, 21, 306, 45}, 12, rgb(27, 37, 18), rgb(55, 71, 29), 1)
	drawLabel(hdc, "v"+appVersion, rect{250, 21, 304, 45}, app.fonts.small, colorAccent, dtCenter|dtSingleLine|dtVCenter)

	status := "USB WAITING"
	statusColor := colorMuted
	if len(app.ports) > 0 {
		status, statusColor = "USB ONLINE", colorGood
	}
	roundedBox(hdc, rect{width - 145, 26, width - 137, 34}, 4, statusColor, statusColor, 1)
	drawLabel(hdc, status, rect{width - 128, 16, width - 24, 44}, app.fonts.tiny, statusColor, dtRight|dtSingleLine|dtVCenter)
}

func (app *clockBuilder) drawHero(hdc uintptr, width int32) {
	area := rect{24, 76, width - 24, 190}
	outlineRoundedBox(hdc, rect{22, 74, width - 22, 192}, 22, rgb(43, 54, 31), 4)
	roundedBox(hdc, area, 20, colorPanel, colorStrong, 1)
	drawLabel(hdc, "USB FIRMWARE STUDIO", rect{47, 93, 360, 111}, app.fonts.tiny, colorAccent, dtLeft|dtSingleLine|dtVCenter)
	drawLabel(hdc, "A safer route from firmware to clock.", rect{47, 112, width - 370, 148}, app.fonts.hero, colorText, dtLeft|dtSingleLine|dtVCenter|dtEndEllipsis)
	drawLabel(hdc, "Identify the ESP32, validate a complete image, then flash with live progress.", rect{47, 151, width - 370, 174}, app.fonts.small, colorMuted, dtLeft|dtSingleLine|dtVCenter|dtEndEllipsis)

	display := rect{width - 340, 91, width - 45, 174}
	outlineRoundedBox(hdc, rect{display.Left - 3, display.Top - 3, display.Right + 3, display.Bottom + 3}, 15, rgb(20, 70, 76), 3)
	roundedBox(hdc, display, 13, rgb(4, 9, 12), rgb(23, 76, 83), 1)
	drawLabel(hdc, "LIVE STATUS", rect{display.Left + 16, display.Top + 10, display.Right - 30, display.Top + 25}, app.fonts.tiny, colorMuted, dtLeft|dtSingleLine|dtVCenter)
	color := app.displayColor()
	roundedBox(hdc, rect{display.Right - 24, display.Top + 14, display.Right - 17, display.Top + 21}, 4, color, color, 1)
	drawLabel(hdc, app.displayTitle(), rect{display.Left + 16, display.Top + 30, display.Right - 14, display.Top + 56}, app.fonts.display, color, dtLeft|dtSingleLine|dtVCenter|dtEndEllipsis)
	drawLabel(hdc, app.displayDetail(), rect{display.Left + 16, display.Top + 58, display.Right - 14, display.Bottom - 8}, app.fonts.tiny, colorMuted, dtLeft|dtWordBreak|dtEndEllipsis)
}

func (app *clockBuilder) drawCardHeading(hdc uintptr, step, heading string, area rect) {
	roundedBox(hdc, area, 17, colorPanel, colorLine, 1)
	roundedBox(hdc, rect{area.Left + 18, area.Top + 15, area.Left + 42, area.Top + 39}, 7, colorAccent, colorAccent, 1)
	drawLabel(hdc, step, rect{area.Left + 18, area.Top + 15, area.Left + 42, area.Top + 39}, app.fonts.small, colorAccentInk, dtCenter|dtSingleLine|dtVCenter)
	drawLabel(hdc, strings.ToUpper(heading), rect{area.Left + 53, area.Top + 14, area.Left + 290, area.Top + 40}, app.fonts.small, colorText, dtLeft|dtSingleLine|dtVCenter)
	line(hdc, area.Left+235, area.Top+27, area.Right-18, area.Top+27, colorLine, 1)
}

func (app *clockBuilder) drawDeviceCard(hdc uintptr, width int32) {
	area := rect{24, 205, width - 24, 342}
	app.drawCardHeading(hdc, "1", "Connect & identify", area)
	app.drawButton(hdc, "refresh", "↻", rect{width - 206, 246, width - 164, 280}, false, !app.busy)
	app.drawButton(hdc, "identify", "Identify", rect{width - 153, 246, width - 43, 280}, false, !app.busy && app.selectedPort >= 0)

	message, tone := "Connect the clock with a data-capable USB cable.", colorCyan
	if app.stage == stageIdentifying {
		message = "Reading chip identity and flash capacity…"
	}
	if app.deviceProblem != "" {
		message, tone = app.deviceProblem, colorDanger
	} else if app.device != nil {
		message, tone = app.device.summary(), colorGood
	} else if len(app.ports) > 0 {
		message = "Press Identify to confirm that the connected module is a classic ESP32."
	}
	app.drawHint(hdc, rect{43, 291, width - 43, 326}, message, tone)
}

func (app *clockBuilder) drawFirmwareCard(hdc uintptr, width int32) {
	area := rect{24, 355, width - 24, 532}
	app.drawCardHeading(hdc, "2", "Choose firmware", area)
	app.drawSegment(hdc, "sourceFolder", "Folder", rect{43, 395, (width / 2) - 4, 426}, !app.fileMode)
	app.drawSegment(hdc, "sourceFile", "Single file", rect{(width / 2) - 4, 395, width - 43, 426}, app.fileMode)
	browseTitle := "Choose Folder…"
	if app.fileMode {
		browseTitle = "Choose File…"
	}
	app.drawButton(hdc, "browse", browseTitle, rect{width - 180, 438, width - 43, 472}, false, !app.busy)
	drawLabel(hdc, app.folder, rect{43, 474, width - 43, 492}, app.fonts.tiny, colorMuted, dtLeft|dtSingleLine|dtVCenter|dtEndEllipsis)

	message, tone := "Choose a full SmartClock_vX.XX.bin image.", colorCyan
	if image := app.currentImage(); image != nil {
		if image.Problem != "" {
			message, tone = image.Problem, colorDanger
		} else {
			message, tone = image.Verdict, colorGood
		}
	} else if !app.fileMode {
		message = "No .bin files were found in this folder."
	}
	app.drawHint(hdc, rect{43, 494, width - 43, 521}, message, tone)
}

func (app *clockBuilder) drawWriteCard(hdc uintptr, width int32) {
	area := rect{24, 545, width - 24, 718}
	app.drawCardHeading(hdc, "3", "Flash clock", area)
	app.drawChoice(hdc, "modeUpdate", "Update — keep settings and WiFi", rect{48, 590, 350, 616}, !app.eraseEverything)
	app.drawChoice(hdc, "modeErase", "Erase everything, then flash", rect{48, 620, 350, 646}, app.eraseEverything)

	flashTitle := "Flash Clock"
	action := "flash"
	if app.stage == stageFlashing {
		flashTitle, action = "Cancel write", "cancel"
	}
	if app.eraseEverything && app.stage != stageFlashing {
		flashTitle = "Erase and Flash"
	}
	app.drawButton(hdc, action, flashTitle, rect{width - 328, 585, width - 158, 627}, true,
		app.stage == stageFlashing || app.canFlash())
	drawLabel(hdc, "SPEED", rect{width - 145, 573, width - 43, 589}, app.fonts.tiny, colorMuted, dtLeft|dtSingleLine|dtVCenter)

	if app.stage == stageFlashing || app.progress > 0 {
		track := rect{48, 661, width - 190, 670}
		roundedBox(hdc, track, 5, colorWell, colorLine, 1)
		fillWidth := int32(float64(track.Right-track.Left) * app.progress)
		if fillWidth > 0 {
			roundedBox(hdc, rect{track.Left, track.Top, track.Left + fillWidth, track.Bottom}, 5, colorAccent, colorAccent, 1)
		}
		drawLabel(hdc, fmt.Sprintf("%d%%", int(app.progress*100)), rect{width - 178, 650, width - 124, 680}, app.fonts.bodyBold, colorAccent, dtRight|dtSingleLine|dtVCenter)
	}

	message, tone := "Identify the ESP32 and choose a validated full image to unlock flashing.", colorCyan
	if app.stage == stageDone {
		message, tone = app.statusMessage, colorGood
	} else if app.stage == stageFailed {
		message, tone = app.statusMessage, colorDanger
	} else if app.canFlash() {
		message, tone = "Ready. The update mode leaves the SPIFFS settings partition untouched.", colorGood
	}
	app.drawHint(hdc, rect{48, 681, width - 43, 709}, message, tone)
}

func (app *clockBuilder) drawLog(hdc uintptr, width int32) {
	if !app.showLog {
		return
	}
	area := rect{24, 731, width - 24, 830}
	roundedBox(hdc, area, 15, colorPanel, colorLine, 1)
	drawLabel(hdc, "DIAGNOSTIC LOG", rect{43, 739, width - 43, 758}, app.fonts.tiny, colorMuted, dtLeft|dtSingleLine|dtVCenter)
}

func (app *clockBuilder) drawFooter(hdc uintptr, width, height int32) {
	top := height - 44
	line(hdc, 0, top, width, top, colorLine, 1)
	app.drawChoice(hdc, "showLog", "Show diagnostic log", rect{27, top + 9, 215, top + 35}, app.showLog)
	drawLabel(hdc, "▣   Full images only  ·  protected write at 0x0", rect{width - 390, top + 8, width - 25, top + 36}, app.fonts.tiny, colorMuted, dtRight|dtSingleLine|dtVCenter)
}

func (app *clockBuilder) positionControls(width int32) {
	moveCombo(app.portCombo, rect{43, 246, width - 218, 280})
	moveCombo(app.imageCombo, rect{43, 438, width - 191, 472})
	moveCombo(app.baudCombo, rect{width - 145, 592, width - 43, 626})
	moveWindow(app.logEdit, rect{43, 759, width - 43, 816}, app.showLog)
	for _, control := range []uintptr{app.portCombo, app.imageCombo, app.baudCombo} {
		enabled := uintptr(1)
		if app.busy {
			enabled = 0
		}
		procEnableWindow.Call(control, enabled)
	}
}

func (app *clockBuilder) drawButton(hdc uintptr, action, title string, area rect, primary, enabled bool) {
	app.buttons[action] = area
	hovered := app.hovered == action
	if primary {
		base, ink := colorAccent, colorAccentInk
		if app.eraseEverything || action == "cancel" {
			base, ink = colorDanger, colorText
		}
		if !enabled {
			base, ink = rgb(63, 69, 55), rgb(126, 132, 118)
		} else {
			outlineRoundedBox(hdc, rect{area.Left - 4, area.Top - 4, area.Right + 4, area.Bottom + 4}, 13, rgb(52, 62, 31), 4)
		}
		if hovered && enabled {
			outlineRoundedBox(hdc, rect{area.Left - 2, area.Top - 2, area.Right + 2, area.Bottom + 2}, 12, base, 2)
		}
		roundedBox(hdc, area, 11, base, base, 1)
		drawLabel(hdc, title, area, app.fonts.bodyBold, ink, dtCenter|dtSingleLine|dtVCenter)
		return
	}
	fill, border, text := colorWell, colorLine, colorText
	if hovered && enabled {
		fill, border = colorRaised, rgb(43, 107, 117)
	}
	if !enabled {
		text = rgb(85, 91, 101)
	}
	roundedBox(hdc, area, 10, fill, border, 1)
	drawLabel(hdc, title, area, app.fonts.bodyBold, text, dtCenter|dtSingleLine|dtVCenter)
}

func (app *clockBuilder) drawSegment(hdc uintptr, action, title string, area rect, selected bool) {
	app.buttons[action] = area
	fill, text, border := colorWell, colorMuted, colorLine
	if selected {
		fill, text, border = rgb(42, 49, 58), colorText, colorStrong
	}
	if app.hovered == action && !selected {
		border = rgb(43, 107, 117)
	}
	roundedBox(hdc, area, 7, fill, border, 1)
	drawLabel(hdc, title, area, app.fonts.body, text, dtCenter|dtSingleLine|dtVCenter)
}

func (app *clockBuilder) drawChoice(hdc uintptr, action, title string, area rect, selected bool) {
	app.buttons[action] = area
	outer := rect{area.Left, area.Top + 5, area.Left + 16, area.Top + 21}
	roundedBox(hdc, outer, 8, colorWell, selectedColor(selected), 1)
	if selected {
		roundedBox(hdc, rect{outer.Left + 4, outer.Top + 4, outer.Right - 4, outer.Bottom - 4}, 4, colorAccent, colorAccent, 1)
	}
	drawLabel(hdc, title, rect{area.Left + 25, area.Top, area.Right, area.Bottom}, app.fonts.small, selectedTextColor(selected), dtLeft|dtSingleLine|dtVCenter)
}

func selectedColor(selected bool) uintptr {
	if selected {
		return colorAccent
	}
	return colorStrong
}
func selectedTextColor(selected bool) uintptr {
	if selected {
		return colorText
	}
	return colorMuted
}

func (app *clockBuilder) drawHint(hdc uintptr, area rect, message string, tone uintptr) {
	fill := colorWell
	if tone == colorDanger {
		fill = rgb(37, 18, 23)
	} else if tone == colorGood {
		fill = rgb(19, 36, 29)
	} else {
		fill = rgb(12, 30, 36)
	}
	roundedBox(hdc, area, 9, fill, colorLine, 1)
	roundedBox(hdc, rect{area.Left + 11, area.Top + 11, area.Left + 18, area.Top + 18}, 4, tone, tone, 1)
	drawLabel(hdc, message, rect{area.Left + 28, area.Top + 4, area.Right - 10, area.Bottom - 4}, app.fonts.small,
		func() uintptr {
			if tone == colorCyan {
				return colorMuted
			}
			return colorText
		}(), dtLeft|dtSingleLine|dtVCenter|dtEndEllipsis)
}

func (app *clockBuilder) displayTitle() string {
	switch app.stage {
	case stageIdentifying:
		return "READING CHIP"
	case stageFlashing:
		return fmt.Sprintf("WRITING  %d%%", int(app.progress*100))
	case stageDone:
		return "FLASH COMPLETE"
	case stageFailed:
		return "ATTENTION"
	default:
		if app.device != nil {
			return "READY TO FLASH"
		}
		if len(app.ports) == 0 {
			return "AWAITING USB"
		}
		return "DEVICE FOUND"
	}
}

func (app *clockBuilder) displayDetail() string {
	switch app.stage {
	case stageIdentifying:
		return "Checking ESP32 family and flash capacity"
	case stageFlashing:
		if image := app.currentImage(); image != nil {
			return fmt.Sprintf("%d / %d KB", int64(app.progress*float64(image.ByteCount))/1024, image.ByteCount/1024)
		}
		return "Writing firmware"
	case stageDone, stageFailed:
		return app.statusMessage
	default:
		if app.device != nil {
			return app.device.summary()
		}
		if len(app.ports) == 0 {
			return "Connect the clock with a data-capable USB cable"
		}
		return "Identify the connected hardware to continue"
	}
}

func (app *clockBuilder) displayColor() uintptr {
	switch app.stage {
	case stageFailed:
		return colorDanger
	case stageIdentifying:
		return colorCyan
	case stageFlashing, stageDone:
		return colorAccent
	default:
		if app.device != nil {
			return colorGood
		}
		return colorCyan
	}
}

func (app *clockBuilder) currentImage() *firmwareImage {
	if app.selectedImage < 0 || app.selectedImage >= len(app.images) {
		return nil
	}
	return &app.images[app.selectedImage]
}

func (app *clockBuilder) canFlash() bool {
	image := app.currentImage()
	return !app.busy && app.selectedPort >= 0 && app.selectedPort < len(app.ports) && image != nil && image.flashable() && app.device != nil && app.deviceProblem == ""
}

func (app *clockBuilder) mouseMove(x, y int32) {
	app.mu.Lock()
	current := app.actionAt(x, y)
	changed := current != app.hovered
	app.hovered = current
	app.mu.Unlock()
	if changed {
		procInvalidateRect.Call(app.window, 0, 0)
	}
}

func (app *clockBuilder) mouseDown(x, y int32) {
	app.mu.Lock()
	app.pressed = app.actionAt(x, y)
	app.mu.Unlock()
	procSetCapture.Call(app.window)
}

func (app *clockBuilder) mouseUp(x, y int32) {
	procReleaseCapture.Call()
	app.mu.Lock()
	action := app.actionAt(x, y)
	pressed := app.pressed
	app.pressed = ""
	app.mu.Unlock()
	if action != "" && action == pressed {
		app.perform(action)
	}
}

func (app *clockBuilder) actionAt(x, y int32) string {
	for action, area := range app.buttons {
		if contains(area, x, y) {
			return action
		}
	}
	return ""
}

func (app *clockBuilder) command(id, notification int) {
	if notification != cbnSelChange {
		return
	}
	app.mu.Lock()
	switch id {
	case idPortCombo:
		app.selectedPort = comboSelection(app.portCombo)
		app.device = nil
		app.deviceProblem = ""
	case idImageCombo:
		app.selectedImage = comboSelection(app.imageCombo)
		app.revalidateLocked()
	case idBaudCombo:
		selection := comboSelection(app.baudCombo)
		choices := []int{460800, 230400, 115200}
		if selection >= 0 && selection < len(choices) {
			app.baud = choices[selection]
		}
	}
	app.mu.Unlock()
	procInvalidateRect.Call(app.window, 0, 0)
}

func (app *clockBuilder) perform(action string) {
	switch action {
	case "refresh":
		app.refreshPorts()
	case "identify":
		app.identify()
	case "sourceFolder":
		app.mu.Lock()
		app.fileMode = false
		app.mu.Unlock()
		app.refreshImages()
	case "sourceFile":
		app.mu.Lock()
		app.fileMode = true
		app.mu.Unlock()
		app.chooseFile()
	case "browse":
		app.mu.Lock()
		fileMode := app.fileMode
		app.mu.Unlock()
		if fileMode {
			app.chooseFile()
		} else {
			app.chooseFolder()
		}
	case "modeUpdate":
		app.mu.Lock()
		if !app.busy {
			app.eraseEverything = false
		}
		app.mu.Unlock()
	case "modeErase":
		app.mu.Lock()
		if !app.busy {
			app.eraseEverything = true
		}
		app.mu.Unlock()
	case "flash":
		app.flash()
	case "cancel":
		app.cancel()
	case "showLog":
		app.mu.Lock()
		app.showLog = !app.showLog
		app.mu.Unlock()
		app.refreshFromWorker()
	}
	procInvalidateRect.Call(app.window, 0, 0)
}

func (app *clockBuilder) refreshPorts() {
	ports := discoverPorts()
	app.mu.Lock()
	previous := ""
	if app.selectedPort >= 0 && app.selectedPort < len(app.ports) {
		previous = app.ports[app.selectedPort]
	}
	app.ports = ports
	app.selectedPort = -1
	for index, port := range ports {
		if port == previous {
			app.selectedPort = index
		}
	}
	if app.selectedPort < 0 && len(ports) > 0 {
		app.selectedPort = 0
	}
	app.device = nil
	app.deviceProblem = ""
	selected := app.selectedPort
	app.mu.Unlock()
	values := ports
	if len(values) == 0 {
		values = []string{"No ESP32 detected"}
		selected = 0
	}
	comboReset(app.portCombo, values, selected)
	procInvalidateRect.Call(app.window, 0, 0)
}

func (app *clockBuilder) refreshImages() {
	app.mu.Lock()
	folder := app.folder
	fileMode := app.fileMode
	app.mu.Unlock()
	if fileMode {
		return
	}
	images := discoverImages(folder)
	app.mu.Lock()
	app.images = images
	app.selectedImage = -1
	for index := range images {
		if images[index].flashable() {
			app.selectedImage = index
			break
		}
	}
	app.revalidateLocked()
	selected := app.selectedImage
	app.mu.Unlock()
	values := make([]string, len(images))
	for index, image := range images {
		values[index] = image.Name
	}
	if len(values) == 0 {
		values = []string{"No .bin files found"}
		selected = 0
	}
	comboReset(app.imageCombo, values, selected)
	procInvalidateRect.Call(app.window, 0, 0)
}

func (app *clockBuilder) chooseFile() {
	app.mu.Lock()
	folder := app.folder
	busy := app.busy
	app.mu.Unlock()
	if busy {
		return
	}
	path := chooseFirmwareFile(app.window, folder)
	if path == "" {
		return
	}
	image := inspectFirmware(path)
	app.mu.Lock()
	app.fileMode = true
	app.folder = filepath.Dir(path)
	app.images = []firmwareImage{image}
	app.selectedImage = 0
	app.revalidateLocked()
	app.mu.Unlock()
	comboReset(app.imageCombo, []string{image.Name}, 0)
	procInvalidateRect.Call(app.window, 0, 0)
}

func powershellLiteral(value string) string { return "'" + strings.ReplaceAll(value, "'", "''") + "'" }

func (app *clockBuilder) chooseFolder() {
	app.mu.Lock()
	folder := app.folder
	busy := app.busy
	app.mu.Unlock()
	if busy {
		return
	}
	script := "Add-Type -AssemblyName System.Windows.Forms; $d=New-Object System.Windows.Forms.FolderBrowserDialog; " +
		"$d.Description='Choose the folder containing SmartClock .bin files'; $d.SelectedPath=" + powershellLiteral(folder) + "; " +
		"if($d.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK){[Console]::Write($d.SelectedPath)}"
	command := exec.Command("powershell.exe", "-NoProfile", "-STA", "-WindowStyle", "Hidden", "-Command", script)
	command.SysProcAttr = &syscall.SysProcAttr{CreationFlags: createNoWindow, HideWindow: true}
	output, err := command.Output()
	if err != nil {
		messageBox(app.window, "Choose Folder", "Windows could not open the folder chooser.", mbOK|mbIconError)
		return
	}
	selected := strings.TrimSpace(string(output))
	if selected == "" {
		return
	}
	app.mu.Lock()
	app.folder = selected
	app.fileMode = false
	app.mu.Unlock()
	app.refreshImages()
}

func (app *clockBuilder) revalidateLocked() {
	app.deviceProblem = ""
	if app.device == nil {
		return
	}
	if !app.device.isClassicESP32() {
		app.deviceProblem = "This is not a classic ESP32. SmartClock firmware will not run on S2, S3, C3 or C6 modules."
		return
	}
	image := app.currentImage()
	if image != nil && image.flashable() && app.device.FlashBytes > 0 && app.device.FlashBytes < image.RequiredFlash {
		app.deviceProblem = fmt.Sprintf("The image needs %d MB flash, but this chip reports %s.", image.RequiredFlash/(1024*1024), app.device.flashDescription())
	}
}

func (app *clockBuilder) appendLog(text string) {
	app.mu.Lock()
	app.log += text
	if len(app.log) > 60000 {
		app.log = app.log[len(app.log)-40000:]
	}
	if match := regexp.MustCompile(`\(\s*(\d+)\s*%\)`).FindStringSubmatch(text); len(match) == 2 {
		if value, err := strconv.Atoi(match[1]); err == nil {
			app.progress = float64(value) / 100.0
		}
	}
	app.mu.Unlock()
	procPostMessage.Call(app.window, wmAppRefresh, 0, 0)
}

func (app *clockBuilder) identify() {
	app.mu.Lock()
	if app.busy || app.selectedPort < 0 || app.selectedPort >= len(app.ports) {
		app.mu.Unlock()
		return
	}
	port := app.ports[app.selectedPort]
	app.busy, app.cancelRequested, app.stage, app.progress = true, false, stageIdentifying, 0
	app.device, app.deviceProblem, app.statusMessage = nil, "", ""
	app.log += "\r\n> esptool flash-id on " + port + "\r\n"
	app.mu.Unlock()
	tool, err := locateEsptool()
	if err != nil {
		app.fail(err.Error())
		return
	}
	var captureMu sync.Mutex
	var captured strings.Builder
	err = app.runner.start(tool, esptoolIdentifyArguments(port), func(text string) {
		captureMu.Lock()
		captured.WriteString(text)
		captureMu.Unlock()
		app.appendLog(text)
	}, func(code int) {
		app.mu.Lock()
		cancelled := app.cancelRequested
		app.busy = false
		app.mu.Unlock()
		if cancelled {
			app.setIdle()
			return
		}
		if code != 0 {
			app.fail("Could not talk to the ESP32. Check the USB data cable and close any Serial Monitor using the port.")
			return
		}
		captureMu.Lock()
		output := captured.String()
		captureMu.Unlock()
		device := parseDevice(output)
		app.mu.Lock()
		app.device = &device
		app.stage = stageIdle
		app.revalidateLocked()
		app.mu.Unlock()
		procPostMessage.Call(app.window, wmAppRefresh, 0, 0)
	})
	if err != nil {
		app.fail("Could not start esptool: " + err.Error())
	}
	procInvalidateRect.Call(app.window, 0, 0)
}

func (app *clockBuilder) flash() {
	app.mu.Lock()
	if !app.canFlash() {
		app.mu.Unlock()
		return
	}
	port := app.ports[app.selectedPort]
	image := *app.currentImage()
	baud := app.baud
	erase := app.eraseEverything
	app.mu.Unlock()
	if erase {
		answer := messageBox(app.window, "Erase everything on this ESP32?",
			"This wipes saved settings, WiFi credentials and cached weather before flashing.\r\n\r\nChoose No and use Update to preserve them.",
			mbYesNo|mbIconWarning|mbDefButton2)
		if answer != idYes {
			return
		}
	}
	tool, err := locateEsptool()
	if err != nil {
		app.fail(err.Error())
		return
	}
	app.mu.Lock()
	app.busy, app.cancelRequested, app.stage, app.progress = true, false, stageFlashing, 0
	app.statusMessage = ""
	app.mu.Unlock()
	if erase {
		app.appendLog("\r\n> esptool erase-flash (wipes settings and WiFi)\r\n")
		err = app.runner.start(tool, esptoolEraseArguments(port, baud), app.appendLog, func(code int) {
			app.mu.Lock()
			cancelled := app.cancelRequested
			app.mu.Unlock()
			if cancelled {
				app.setIdle()
				return
			}
			if code != 0 {
				app.mu.Lock()
				app.busy = false
				app.mu.Unlock()
				app.fail("Erase failed before any firmware was written.")
				return
			}
			app.startWrite(tool, port, baud, image)
		})
	} else {
		app.startWrite(tool, port, baud, image)
		return
	}
	if err != nil {
		app.fail("Could not start esptool: " + err.Error())
	}
}

func (app *clockBuilder) startWrite(tool, port string, baud int, image firmwareImage) {
	app.appendLog("\r\n> esptool write-flash 0x0 " + image.Name + "\r\n")
	err := app.runner.start(tool, esptoolWriteArguments(port, baud, image.Path), app.appendLog, func(code int) {
		app.mu.Lock()
		cancelled := app.cancelRequested
		app.busy = false
		if cancelled {
			app.stage, app.progress, app.statusMessage = stageIdle, 0, ""
		} else if code == 0 {
			app.stage, app.progress = stageDone, 1
			app.statusMessage = image.Name + " written. The clock is restarting."
		} else {
			app.stage, app.progress = stageFailed, 0
			app.statusMessage = "Flashing stopped. Try another USB cable or select a lower speed."
		}
		app.mu.Unlock()
		procPostMessage.Call(app.window, wmAppRefresh, 0, 0)
	})
	if err != nil {
		app.fail("Could not start esptool: " + err.Error())
	}
}

func (app *clockBuilder) cancel() {
	app.mu.Lock()
	app.cancelRequested = true
	app.statusMessage = "Cancelling…"
	app.mu.Unlock()
	app.runner.cancel()
	app.appendLog("\r\nCancelled by user.\r\n")
}

func (app *clockBuilder) fail(message string) {
	app.mu.Lock()
	app.busy, app.stage, app.progress, app.statusMessage = false, stageFailed, 0, message
	app.mu.Unlock()
	procPostMessage.Call(app.window, wmAppRefresh, 0, 0)
}

func (app *clockBuilder) setIdle() {
	app.mu.Lock()
	app.busy, app.stage, app.progress, app.statusMessage = false, stageIdle, 0, ""
	app.mu.Unlock()
	procPostMessage.Call(app.window, wmAppRefresh, 0, 0)
}

func (app *clockBuilder) refreshFromWorker() {
	app.mu.Lock()
	logText, show := app.log, app.showLog
	app.mu.Unlock()
	if show {
		procSetWindowText.Call(app.logEdit, uintptr(unsafe.Pointer(utf16Ptr(logText))))
	}
	procInvalidateRect.Call(app.window, 0, 0)
}
