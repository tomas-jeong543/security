// Reconstructed source for reversing_project3(1).exe
//
// Verified from the binary:
//
//	Go version: go1.23.2, GOOS=windows, GOARCH=amd64, CGO_ENABLED=0
//	Functions: rotl32, mix32, hashName, quarter, derive, parseSerial,
//	           hexNibble, vmCheck, antiDebug, decoy, main
//
// This is a buildable behavioral reconstruction. Compilation does not restore
// comments, local variable names, or every original source-level expression.
package main

import (
	"bufio"
	"crypto/subtle"
	"fmt"
	"os"
	"strings"
	"syscall"
	"unsafe"
)

const (
	minUserIDLength = 3
	maxUserIDLength = 24
	serialByteCount = 16
)

// rotl32 rotates a 32-bit value to the left.
func rotl32(x uint32, n uint32) uint32 {
	n &= 31
	if n == 0 {
		return x
	}
	return (x << n) | (x >> (32 - n))
}

// mix32 is the 32-bit avalanche mixer visible in the executable.
func mix32(x uint32) uint32 {
	x = (x ^ (x >> 16)) * 0x7FEB352D
	x = (x ^ (x >> 15)) * 0x846CA68B
	return x ^ (x >> 16)
}

// hashName hashes the normalized user ID and then avalanches the result.
func hashName(name string) uint32 {
	const (
		fnvOffset = uint32(0x811C9DC5)
		fnvPrime  = uint32(0x01000193)
	)

	h := fnvOffset
	for i := 0; i < len(name); i++ {
		h ^= uint32(name[i])
		h *= fnvPrime
	}

	return mix32(h ^ 0x045D9F3B)
}

// quarter performs one ARX mixing round over four 32-bit words.
func quarter(a, b, c, d *uint32) {
	*a += *b
	*d ^= *a
	*d = rotl32(*d, 16)

	*c += *d
	*b ^= *c
	*b = rotl32(*b, 12)

	*a += *b
	*d ^= *a
	*d = rotl32(*d, 8)

	*c += *d
	*b ^= *c
	*b = rotl32(*b, 7)
}

// derive creates the 16-byte expected license value from the normalized ID.
func derive(name string) [serialByteCount]byte {
	h := hashName(name)

	s0 := h ^ 0x61707865
	s1 := rotl32(h, 7) ^ 0x3320646E
	s2 := mix32(h ^ 0x79622D32)
	s3 := uint32(len(name))*0x9E3779B1 ^ 0x6B206574

	for round := uint32(0); round < 6; round++ {
		quarter(&s0, &s1, &s2, &s3)
		s0 ^= round*0xA5A5A5A5 + 0x13579BDF
		s2 = rotl32(s2^h, ((3*round+5)%31)+1)
	}

	words := [4]uint32{
		s0 ^ mix32(s2),
		s1 + rotl32(s3, 3),
		s2 ^ rotl32(s0, 11),
		s3 + mix32(s1),
	}

	var out [serialByteCount]byte
	for i, word := range words {
		base := i * 4
		out[base] = byte(word)
		out[base+1] = byte(word >> 8)
		out[base+2] = byte(word >> 16)
		out[base+3] = byte(word >> 24)
	}

	for i := range out {
		v := out[i] ^ byte(29*i+83)
		out[i] = (v << 3) | (v >> 5)
	}

	return out
}

// hexNibble converts one hexadecimal character to a value from 0 to 15.
func hexNibble(c byte) (byte, bool) {
	switch {
	case c >= '0' && c <= '9':
		return c - '0', true
	case c >= 'A' && c <= 'F':
		return c - 'A' + 10, true
	case c >= 'a' && c <= 'f':
		return c - 'a' + 10, true
	default:
		return 0, false
	}
}

// parseSerial accepts 32 hexadecimal characters. Hyphens are ignored.
func parseSerial(text string) ([serialByteCount]byte, bool) {
	var out [serialByteCount]byte

	text = strings.TrimSpace(text)
	text = strings.ReplaceAll(text, "-", "")
	if len(text) != serialByteCount*2 {
		return out, false
	}

	for i := 0; i < serialByteCount; i++ {
		hi, okHi := hexNibble(text[i*2])
		lo, okLo := hexNibble(text[i*2+1])
		if !okHi || !okLo {
			return out, false
		}
		out[i] = hi<<4 | lo
	}

	return out, true
}

// vmCheck models the verifier as a small byte-code interpreter rather than a
// direct equality branch. The accumulator becomes zero only when all bytes
// match, and subtle.ConstantTimeByteEq avoids an obvious early-exit compare.
func vmCheck(expected, supplied [serialByteCount]byte) bool {
	const (
		opLoadExpected = byte(0x11)
		opLoadSupplied = byte(0x22)
		opXor          = byte(0x33)
		opOrAccum      = byte(0x44)
		opAdvance      = byte(0x55)
		opLoop         = byte(0x66)
		opFinish       = byte(0x77)
	)

	program := [...]byte{
		opLoadExpected,
		opLoadSupplied,
		opXor,
		opOrAccum,
		opAdvance,
		opLoop,
		opFinish,
	}

	var (
		pc    int
		index int
		left  byte
		right byte
		diff  byte
	)

	for {
		switch program[pc] {
		case opLoadExpected:
			left = expected[index]
			pc++
		case opLoadSupplied:
			right = supplied[index]
			pc++
		case opXor:
			diff |= left ^ right
			pc++
		case opOrAccum:
			// Mix a constant-time equality result into the interpreter state.
			diff |= byte(subtle.ConstantTimeByteEq(left, right) ^ 1)
			pc++
		case opAdvance:
			index++
			pc++
		case opLoop:
			if index < serialByteCount {
				pc = 0
			} else {
				pc++
			}
		case opFinish:
			return diff == 0
		default:
			return false
		}
	}
}

// antiDebug performs the Windows debugger checks used by this reconstruction.
func antiDebug() bool {
	kernel32 := syscall.NewLazyDLL("kernel32.dll")
	isDebuggerPresent := kernel32.NewProc("IsDebuggerPresent")
	checkRemoteDebuggerPresent := kernel32.NewProc("CheckRemoteDebuggerPresent")
	getCurrentProcess := kernel32.NewProc("GetCurrentProcess")

	if present, _, _ := isDebuggerPresent.Call(); present != 0 {
		return true
	}

	process, _, _ := getCurrentProcess.Call()
	var remoteDebuggerPresent int32
	ok, _, _ := checkRemoteDebuggerPresent.Call(
		process,
		uintptr(unsafe.Pointer(&remoteDebuggerPresent)),
	)

	return ok != 0 && remoteDebuggerPresent != 0
}

// decoy derives a deliberately different target when debugging is detected.
func decoy(name string, supplied [serialByteCount]byte) bool {
	fake := derive(name + "#debug")
	return vmCheck(fake, supplied)
}

func main() {
	reader := bufio.NewReader(os.Stdin)

	fmt.Println("RVP3 License Validation Console")
	fmt.Print("User ID: ")
	userID, _ := reader.ReadString('\n')
	userID = strings.ToLower(strings.TrimSpace(userID))

	fmt.Print("License: ")
	serialText, _ := reader.ReadString('\n')

	supplied, validFormat := parseSerial(serialText)
	if !validFormat || len(userID) < minUserIDLength || len(userID) > maxUserIDLength {
		fmt.Println("Invalid Format")
		return
	}

	var accepted bool
	if antiDebug() {
		accepted = decoy(userID, supplied)
	} else {
		expected := derive(userID)
		accepted = vmCheck(expected, supplied)
	}

	if accepted {
		fmt.Println("License Accepted")
	} else {
		fmt.Println("Access Denied")
	}
}
