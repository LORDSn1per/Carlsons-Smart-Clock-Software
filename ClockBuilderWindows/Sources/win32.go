//go:build windows

package main

import (
	"syscall"
	"unicode/utf16"
	"unsafe"
)

type rect struct{ Left, Top, Right, Bottom int32 }
type point struct{ X, Y int32 }

type msg struct {
	Hwnd    uintptr
	Message uint32
	WParam  uintptr
	LParam  uintptr
	Time    uint32
	Pt      point
	Private uint32
}

type paintStruct struct {
	Hdc         uintptr
	Erase       int32
	Paint       rect
	Restore     int32
	IncUpdate   int32
	RGBReserved [32]byte
}

type wndClassEx struct {
	Size       uint32
	Style      uint32
	WndProc    uintptr
	ClsExtra   int32
	WndExtra   int32
	Instance   uintptr
	Icon       uintptr
	Cursor     uintptr
	Background uintptr
	MenuName   *uint16
	ClassName  *uint16
	IconSmall  uintptr
}

type openFilename struct {
	StructSize      uint32
	Owner           uintptr
	Instance        uintptr
	Filter          *uint16
	CustomFilter    *uint16
	MaxCustomFilter uint32
	FilterIndex     uint32
	File            *uint16
	MaxFile         uint32
	FileTitle       *uint16
	MaxFileTitle    uint32
	InitialDir      *uint16
	Title           *uint16
	Flags           uint32
	FileOffset      uint16
	FileExtension   uint16
	DefExt          *uint16
	CustData        uintptr
	Hook            uintptr
	TemplateName    *uint16
	Reserved        uintptr
	ReservedDWord   uint32
	FlagsEx         uint32
}

const (
	wmCreate          = 0x0001
	wmDestroy         = 0x0002
	wmSize            = 0x0005
	wmPaint           = 0x000F
	wmClose           = 0x0010
	wmCommand         = 0x0111
	wmSetFont         = 0x0030
	wmCtlColorEdit    = 0x0133
	wmCtlColorListBox = 0x0134
	wmMouseMove       = 0x0200
	wmLButtonDown     = 0x0201
	wmLButtonUp       = 0x0202
	wmAppRefresh      = 0x8001

	wsOverlappedWindow = 0x00CF0000
	wsClipChildren     = 0x02000000
	wsChild            = 0x40000000
	wsVisible          = 0x10000000
	wsTabStop          = 0x00010000
	wsVScroll          = 0x00200000
	wsBorder           = 0x00800000

	cbsDropdownList = 0x0003
	cbsHasStrings   = 0x0200
	esMultiline     = 0x0004
	esReadOnly      = 0x0800
	esAutoVScroll   = 0x0040

	cbAddString    = 0x0143
	cbResetContent = 0x014B
	cbGetCurSel    = 0x0147
	cbSetCurSel    = 0x014E
	cbnSelChange   = 1

	swHide = 0
	swShow = 5

	dtLeft        = 0x0000
	dtCenter      = 0x0001
	dtRight       = 0x0002
	dtVCenter     = 0x0004
	dtSingleLine  = 0x0020
	dtWordBreak   = 0x0010
	dtEndEllipsis = 0x8000

	transparent = 1
	psSolid     = 0
	psNull      = 5
	nullBrush   = 5

	mbOK          = 0x00000000
	mbIconError   = 0x00000010
	mbIconWarning = 0x00000030
	mbYesNo       = 0x00000004
	mbDefButton2  = 0x00000100
	idYes         = 6

	ofnExplorer      = 0x00080000
	ofnFileMustExist = 0x00001000
	ofnPathMustExist = 0x00000800
	ofnNoChangeDir   = 0x00000008

	gwlpUserData = -21
)

var (
	user32   = syscall.NewLazyDLL("user32.dll")
	gdi32    = syscall.NewLazyDLL("gdi32.dll")
	kernel32 = syscall.NewLazyDLL("kernel32.dll")
	comdlg32 = syscall.NewLazyDLL("comdlg32.dll")
	dwmapi   = syscall.NewLazyDLL("dwmapi.dll")
	uxtheme  = syscall.NewLazyDLL("uxtheme.dll")

	procRegisterClassEx  = user32.NewProc("RegisterClassExW")
	procCreateWindowEx   = user32.NewProc("CreateWindowExW")
	procDefWindowProc    = user32.NewProc("DefWindowProcW")
	procShowWindow       = user32.NewProc("ShowWindow")
	procUpdateWindow     = user32.NewProc("UpdateWindow")
	procGetMessage       = user32.NewProc("GetMessageW")
	procTranslateMessage = user32.NewProc("TranslateMessage")
	procDispatchMessage  = user32.NewProc("DispatchMessageW")
	procPostQuitMessage  = user32.NewProc("PostQuitMessage")
	procBeginPaint       = user32.NewProc("BeginPaint")
	procEndPaint         = user32.NewProc("EndPaint")
	procGetClientRect    = user32.NewProc("GetClientRect")
	procFillRect         = user32.NewProc("FillRect")
	procInvalidateRect   = user32.NewProc("InvalidateRect")
	procSetWindowPos     = user32.NewProc("SetWindowPos")
	procEnableWindow     = user32.NewProc("EnableWindow")
	procSendMessage      = user32.NewProc("SendMessageW")
	procPostMessage      = user32.NewProc("PostMessageW")
	procSetCapture       = user32.NewProc("SetCapture")
	procReleaseCapture   = user32.NewProc("ReleaseCapture")
	procLoadCursor       = user32.NewProc("LoadCursorW")
	procMessageBox       = user32.NewProc("MessageBoxW")
	procSetProcessDPI    = user32.NewProc("SetProcessDPIAware")
	procSetWindowText    = user32.NewProc("SetWindowTextW")

	procCreateSolidBrush = gdi32.NewProc("CreateSolidBrush")
	procCreatePen        = gdi32.NewProc("CreatePen")
	procSelectObject     = gdi32.NewProc("SelectObject")
	procDeleteObject     = gdi32.NewProc("DeleteObject")
	procRoundRect        = gdi32.NewProc("RoundRect")
	procMoveToEx         = gdi32.NewProc("MoveToEx")
	procLineTo           = gdi32.NewProc("LineTo")
	procSetTextColor     = gdi32.NewProc("SetTextColor")
	procSetBkColor       = gdi32.NewProc("SetBkColor")
	procSetBkMode        = gdi32.NewProc("SetBkMode")
	procCreateFont       = gdi32.NewProc("CreateFontW")
	procGetStockObject   = gdi32.NewProc("GetStockObject")

	procDrawText        = user32.NewProc("DrawTextW")
	procGetModuleHandle = kernel32.NewProc("GetModuleHandleW")
	procGetOpenFileName = comdlg32.NewProc("GetOpenFileNameW")
	procDwmSetAttribute = dwmapi.NewProc("DwmSetWindowAttribute")
	procSetWindowTheme  = uxtheme.NewProc("SetWindowTheme")
)

func utf16Ptr(value string) *uint16 {
	pointer, _ := syscall.UTF16PtrFromString(value)
	return pointer
}

func utf16Buffer(value string) []uint16 {
	return append(utf16.Encode([]rune(value)), 0)
}

func rgb(red, green, blue byte) uintptr {
	return uintptr(red) | uintptr(green)<<8 | uintptr(blue)<<16
}

func loWord(value uintptr) uint16 { return uint16(value & 0xFFFF) }
func hiWord(value uintptr) uint16 { return uint16((value >> 16) & 0xFFFF) }

func xFromLParam(value uintptr) int32 { return int32(int16(loWord(value))) }
func yFromLParam(value uintptr) int32 { return int32(int16(hiWord(value))) }

func contains(r rect, x, y int32) bool {
	return x >= r.Left && x < r.Right && y >= r.Top && y < r.Bottom
}

func createBrush(color uintptr) uintptr {
	brush, _, _ := procCreateSolidBrush.Call(color)
	return brush
}

func createFont(height int32, weight int32, face string) uintptr {
	font, _, _ := procCreateFont.Call(
		uintptr(height), 0, 0, 0, uintptr(weight), 0, 0, 0,
		1, 0, 0, 5, 0, uintptr(unsafe.Pointer(utf16Ptr(face))),
	)
	return font
}

func fillRectangle(hdc uintptr, area rect, brush uintptr) {
	procFillRect.Call(hdc, uintptr(unsafe.Pointer(&area)), brush)
}

func roundedBox(hdc uintptr, area rect, radius int32, fill, border uintptr, borderWidth int32) {
	brush := createBrush(fill)
	pen, _, _ := procCreatePen.Call(psSolid, uintptr(borderWidth), border)
	oldBrush, _, _ := procSelectObject.Call(hdc, brush)
	oldPen, _, _ := procSelectObject.Call(hdc, pen)
	procRoundRect.Call(hdc, uintptr(area.Left), uintptr(area.Top), uintptr(area.Right), uintptr(area.Bottom), uintptr(radius), uintptr(radius))
	procSelectObject.Call(hdc, oldBrush)
	procSelectObject.Call(hdc, oldPen)
	procDeleteObject.Call(brush)
	procDeleteObject.Call(pen)
}

func outlineRoundedBox(hdc uintptr, area rect, radius int32, border uintptr, borderWidth int32) {
	pen, _, _ := procCreatePen.Call(psSolid, uintptr(borderWidth), border)
	null, _, _ := procGetStockObject.Call(nullBrush)
	oldBrush, _, _ := procSelectObject.Call(hdc, null)
	oldPen, _, _ := procSelectObject.Call(hdc, pen)
	procRoundRect.Call(hdc, uintptr(area.Left), uintptr(area.Top), uintptr(area.Right), uintptr(area.Bottom), uintptr(radius), uintptr(radius))
	procSelectObject.Call(hdc, oldBrush)
	procSelectObject.Call(hdc, oldPen)
	procDeleteObject.Call(pen)
}

func drawLabel(hdc uintptr, text string, area rect, font, color uintptr, flags uint32) {
	oldFont, _, _ := procSelectObject.Call(hdc, font)
	procSetTextColor.Call(hdc, color)
	procSetBkMode.Call(hdc, transparent)
	pointer := utf16Ptr(text)
	procDrawText.Call(hdc, uintptr(unsafe.Pointer(pointer)), ^uintptr(0), uintptr(unsafe.Pointer(&area)), uintptr(flags))
	procSelectObject.Call(hdc, oldFont)
}

func line(hdc uintptr, x1, y1, x2, y2 int32, color uintptr, width int32) {
	pen, _, _ := procCreatePen.Call(psSolid, uintptr(width), color)
	oldPen, _, _ := procSelectObject.Call(hdc, pen)
	procMoveToEx.Call(hdc, uintptr(x1), uintptr(y1), 0)
	procLineTo.Call(hdc, uintptr(x2), uintptr(y2))
	procSelectObject.Call(hdc, oldPen)
	procDeleteObject.Call(pen)
}

func createChild(className, title string, style uint32, id int, parent, instance uintptr) uintptr {
	handle, _, _ := procCreateWindowEx.Call(
		0,
		uintptr(unsafe.Pointer(utf16Ptr(className))),
		uintptr(unsafe.Pointer(utf16Ptr(title))),
		uintptr(style), 0, 0, 10, 10,
		parent, uintptr(id), instance, 0,
	)
	return handle
}

func moveWindow(handle uintptr, area rect, show bool) {
	flags := uintptr(0x0010 | 0x0004) // SWP_NOACTIVATE | SWP_NOZORDER
	procSetWindowPos.Call(handle, 0, uintptr(area.Left), uintptr(area.Top), uintptr(area.Right-area.Left), uintptr(area.Bottom-area.Top), flags)
	if show {
		procShowWindow.Call(handle, swShow)
	} else {
		procShowWindow.Call(handle, swHide)
	}
}

// For CBS_DROPDOWNLIST, the height passed to SetWindowPos includes the hidden
// drop-down list. Giving it only the visible 34 pixels produces a menu with no
// useful rows on some Windows versions.
func moveCombo(handle uintptr, area rect) {
	flags := uintptr(0x0010 | 0x0004) // SWP_NOACTIVATE | SWP_NOZORDER
	procSetWindowPos.Call(handle, 0, uintptr(area.Left), uintptr(area.Top), uintptr(area.Right-area.Left), 220, flags)
	procShowWindow.Call(handle, swShow)
}

func comboReset(handle uintptr, values []string, selection int) {
	procSendMessage.Call(handle, cbResetContent, 0, 0)
	for _, value := range values {
		procSendMessage.Call(handle, cbAddString, 0, uintptr(unsafe.Pointer(utf16Ptr(value))))
	}
	if selection >= 0 && selection < len(values) {
		procSendMessage.Call(handle, cbSetCurSel, uintptr(selection), 0)
	}
}

func comboSelection(handle uintptr) int {
	selection, _, _ := procSendMessage.Call(handle, cbGetCurSel, 0, 0)
	if int32(selection) < 0 {
		return -1
	}
	return int(selection)
}

func chooseFirmwareFile(owner uintptr, initialDir string) string {
	buffer := make([]uint16, 32768)
	filter := make([]uint16, 0, 80)
	for _, part := range []string{"ESP32 firmware (*.bin)", "*.bin", "All files (*.*)", "*.*"} {
		filter = append(filter, utf16.Encode([]rune(part))...)
		filter = append(filter, 0)
	}
	filter = append(filter, 0)
	directory := utf16Ptr(initialDir)
	title := utf16Ptr("Choose a full SmartClock firmware image")
	definition := utf16Ptr("bin")
	request := openFilename{
		StructSize:  uint32(unsafe.Sizeof(openFilename{})),
		Owner:       owner,
		Filter:      &filter[0],
		FilterIndex: 1,
		File:        &buffer[0],
		MaxFile:     uint32(len(buffer)),
		InitialDir:  directory,
		Title:       title,
		Flags:       ofnExplorer | ofnFileMustExist | ofnPathMustExist | ofnNoChangeDir,
		DefExt:      definition,
	}
	result, _, _ := procGetOpenFileName.Call(uintptr(unsafe.Pointer(&request)))
	if result == 0 {
		return ""
	}
	return syscall.UTF16ToString(buffer)
}

func messageBox(owner uintptr, title, body string, flags uintptr) int {
	result, _, _ := procMessageBox.Call(owner, uintptr(unsafe.Pointer(utf16Ptr(body))), uintptr(unsafe.Pointer(utf16Ptr(title))), flags)
	return int(result)
}

func setDarkWindow(handle uintptr) {
	value := int32(1)
	procDwmSetAttribute.Call(handle, 20, uintptr(unsafe.Pointer(&value)), unsafe.Sizeof(value))
	procSetWindowTheme.Call(handle, uintptr(unsafe.Pointer(utf16Ptr("DarkMode_Explorer"))), 0)
}
