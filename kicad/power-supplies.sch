EESchema Schematic File Version 2
LIBS:power
LIBS:device
LIBS:switches
LIBS:relays
LIBS:motors
LIBS:transistors
LIBS:conn
LIBS:linear
LIBS:regul
LIBS:74xx
LIBS:cmos4000
LIBS:adc-dac
LIBS:memory
LIBS:xilinx
LIBS:microcontrollers
LIBS:dsp
LIBS:microchip
LIBS:analog_switches
LIBS:motorola
LIBS:texas
LIBS:intel
LIBS:audio
LIBS:interface
LIBS:digital-audio
LIBS:philips
LIBS:display
LIBS:cypress
LIBS:siliconi
LIBS:opto
LIBS:atmel
LIBS:contrib
LIBS:valves
LIBS:midi-fader
LIBS:midi-fader-cache
EELAYER 25 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 3 4
Title "Power Supplies"
Date "2018-07-13"
Rev "A"
Comp "Kevin Cuzner"
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L MCP1603 U3
U 1 1 5B3C69DA
P 7300 2300
F 0 "U3" H 7450 2050 60  0000 C CNN
F 1 "MCP1603" H 7300 2550 60  0000 C CNN
F 2 "TO_SOT_Packages_SMD:TSOT-23-5" H 7300 1900 60  0001 C CNN
F 3 "" H 7300 1900 60  0001 C CNN
F 4 "MCP1603T-330I/OSCT-ND" H 7300 2650 60  0001 C CNN "Part No."
	1    7300 2300
	1    0    0    -1  
$EndComp
$Comp
L MIC5501-XYM5 U5
U 1 1 5B3C6A50
P 2900 3900
F 0 "U5" H 3100 3650 60  0000 C CNN
F 1 "MIC5501-XYM5" H 2900 4150 60  0000 C CNN
F 2 "TO_SOT_Packages_SMD:SOT-23-5" H 2900 3800 60  0001 C CNN
F 3 "" H 2900 3800 60  0001 C CNN
F 4 "576-4764-1-ND" H 2900 4250 60  0001 C CNN "Part No."
	1    2900 3900
	1    0    0    -1  
$EndComp
$Comp
L L L2
U 1 1 5B3CF824
P 8150 2200
F 0 "L2" V 8100 2200 50  0000 C CNN
F 1 "4.7u" V 8225 2200 50  0000 C CNN
F 2 "midi-fader:Wurth_TPC_3816" H 8150 2200 50  0001 C CNN
F 3 "" H 8150 2200 50  0001 C CNN
F 4 "732-1008-1-ND" H 8150 2200 60  0001 C CNN "Part No."
	1    8150 2200
	0    -1   -1   0   
$EndComp
$Comp
L C C18
U 1 1 5B3CF8EC
P 8950 2650
F 0 "C18" H 8975 2750 50  0000 L CNN
F 1 "4.7u" H 8975 2550 50  0000 L CNN
F 2 "Capacitors_SMD:C_0805" H 8988 2500 50  0001 C CNN
F 3 "" H 8950 2650 50  0001 C CNN
F 4 "1276-6464-1-ND" H 8950 2650 60  0001 C CNN "Part No."
	1    8950 2650
	1    0    0    -1  
$EndComp
$Comp
L VBUS #PWR011
U 1 1 5B3CF911
P 1000 2100
F 0 "#PWR011" H 1000 1950 50  0001 C CNN
F 1 "VBUS" H 1000 2250 50  0000 C CNN
F 2 "" H 1000 2100 50  0001 C CNN
F 3 "" H 1000 2100 50  0001 C CNN
	1    1000 2100
	1    0    0    -1  
$EndComp
$Comp
L C C14
U 1 1 5B3CF945
P 1000 2450
F 0 "C14" H 1025 2550 50  0000 L CNN
F 1 "4.7u" H 1025 2350 50  0000 L CNN
F 2 "Capacitors_SMD:C_0805" H 1038 2300 50  0001 C CNN
F 3 "" H 1000 2450 50  0001 C CNN
F 4 "1276-6464-1-ND" H 1000 2450 60  0001 C CNN "Part No."
	1    1000 2450
	1    0    0    -1  
$EndComp
$Comp
L C C15
U 1 1 5B3CF984
P 1300 2450
F 0 "C15" H 1325 2550 50  0000 L CNN
F 1 "1u" H 1325 2350 50  0000 L CNN
F 2 "Capacitors_SMD:C_0603" H 1338 2300 50  0001 C CNN
F 3 "" H 1300 2450 50  0001 C CNN
F 4 "1276-1182-1-ND" H 1300 2450 60  0001 C CNN "Part No."
	1    1300 2450
	1    0    0    -1  
$EndComp
$Comp
L R R15
U 1 1 5B3CF9F9
P 2900 2450
F 0 "R15" V 2980 2450 50  0000 C CNN
F 1 "10K" V 2900 2450 50  0000 C CNN
F 2 "Resistors_SMD:R_0402" V 2830 2450 50  0001 C CNN
F 3 "" H 2900 2450 50  0001 C CNN
	1    2900 2450
	1    0    0    -1  
$EndComp
$Comp
L R R18
U 1 1 5B3CFA6C
P 2900 2950
F 0 "R18" V 2980 2950 50  0000 C CNN
F 1 "750" V 2900 2950 50  0000 C CNN
F 2 "Resistors_SMD:R_0402" V 2830 2950 50  0001 C CNN
F 3 "" H 2900 2950 50  0001 C CNN
	1    2900 2950
	1    0    0    -1  
$EndComp
$Comp
L R R20
U 1 1 5B3CFA99
P 1900 3750
F 0 "R20" V 1980 3750 50  0000 C CNN
F 1 "0" V 1900 3750 50  0000 C CNN
F 2 "Resistors_SMD:R_0402" V 1830 3750 50  0001 C CNN
F 3 "" H 1900 3750 50  0001 C CNN
	1    1900 3750
	1    0    0    -1  
$EndComp
$Comp
L R R22
U 1 1 5B3CFAD8
P 1900 4250
F 0 "R22" V 1980 4250 50  0000 C CNN
F 1 "SPR" V 1900 4250 50  0000 C CNN
F 2 "Resistors_SMD:R_0402" V 1830 4250 50  0001 C CNN
F 3 "" H 1900 4250 50  0001 C CNN
	1    1900 4250
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR012
U 1 1 5B3CFC0A
P 1900 4600
F 0 "#PWR012" H 1900 4350 50  0001 C CNN
F 1 "GND" H 1900 4450 50  0000 C CNN
F 2 "" H 1900 4600 50  0001 C CNN
F 3 "" H 1900 4600 50  0001 C CNN
	1    1900 4600
	1    0    0    -1  
$EndComp
$Comp
L R R14
U 1 1 5B3CFC3E
P 1600 2450
F 0 "R14" V 1680 2450 50  0000 C CNN
F 1 "1K" V 1600 2450 50  0000 C CNN
F 2 "Resistors_SMD:R_0805" V 1530 2450 50  0001 C CNN
F 3 "" H 1600 2450 50  0001 C CNN
	1    1600 2450
	1    0    0    -1  
$EndComp
$Comp
L R R17
U 1 1 5B3CFCD7
P 9300 2650
F 0 "R17" V 9380 2650 50  0000 C CNN
F 1 "220" V 9300 2650 50  0000 C CNN
F 2 "Resistors_SMD:R_1206" V 9230 2650 50  0001 C CNN
F 3 "" H 9300 2650 50  0001 C CNN
	1    9300 2650
	1    0    0    -1  
$EndComp
Text Notes 2100 750  0    60   ~ 0
Intended powerup sequence
Text Notes 2250 950  0    60   ~ 0
VUSB
Text Notes 2250 1300 0    60   ~ 0
VDDA
Text Notes 2300 1600 0    60   ~ 0
VDD
Text Notes 3900 750  0    60   ~ 0
Intended powerdown sequence
Text Notes 3900 950  0    60   ~ 0
VUSB
Text Notes 3900 1300 0    60   ~ 0
VDDA
Text Notes 3900 1600 0    60   ~ 0
VDD
$Comp
L MIC2776L U4
U 1 1 5B3D06F8
P 5500 2800
F 0 "U4" H 5650 2550 60  0000 C CNN
F 1 "MIC2776L" H 5500 3050 60  0000 C CNN
F 2 "TO_SOT_Packages_SMD:SOT-23-5" H 5500 2800 60  0001 C CNN
F 3 "" H 5500 2800 60  0001 C CNN
F 4 "576-2686-1-ND" H 5500 3150 60  0001 C CNN "Part No."
	1    5500 2800
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR013
U 1 1 5B3D0BA6
P 1000 2800
F 0 "#PWR013" H 1000 2550 50  0001 C CNN
F 1 "GND" H 1000 2650 50  0000 C CNN
F 2 "" H 1000 2800 50  0001 C CNN
F 3 "" H 1000 2800 50  0001 C CNN
	1    1000 2800
	1    0    0    -1  
$EndComp
$Comp
L C C20
U 1 1 5B3D0D98
P 3550 4150
F 0 "C20" H 3575 4250 50  0000 L CNN
F 1 "1u" H 3575 4050 50  0000 L CNN
F 2 "Capacitors_SMD:C_0603" H 3588 4000 50  0001 C CNN
F 3 "" H 3550 4150 50  0001 C CNN
F 4 "1276-1182-1-ND" H 3550 4150 60  0001 C CNN "Part No."
	1    3550 4150
	1    0    0    -1  
$EndComp
$Comp
L C C21
U 1 1 5B3D0DE7
P 3800 4150
F 0 "C21" H 3825 4250 50  0000 L CNN
F 1 "1u" H 3825 4050 50  0000 L CNN
F 2 "Capacitors_SMD:C_0603" H 3838 4000 50  0001 C CNN
F 3 "" H 3800 4150 50  0001 C CNN
F 4 "1276-1182-1-ND" H 3800 4150 60  0001 C CNN "Part No."
	1    3800 4150
	1    0    0    -1  
$EndComp
$Comp
L C C19
U 1 1 5B3D1400
P 4500 2900
F 0 "C19" H 4525 3000 50  0000 L CNN
F 1 "0.1u" H 4525 2800 50  0000 L CNN
F 2 "Capacitors_SMD:C_0402" H 4538 2750 50  0001 C CNN
F 3 "" H 4500 2900 50  0001 C CNN
F 4 "1276-1043-1-ND" H 4500 2900 60  0001 C CNN "Part No."
	1    4500 2900
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR014
U 1 1 5B3D1467
P 4500 3150
F 0 "#PWR014" H 4500 2900 50  0001 C CNN
F 1 "GND" H 4500 3000 50  0000 C CNN
F 2 "" H 4500 3150 50  0001 C CNN
F 3 "" H 4500 3150 50  0001 C CNN
	1    4500 3150
	1    0    0    -1  
$EndComp
$Comp
L R R21
U 1 1 5B3D1C03
P 4500 4050
F 0 "R21" V 4580 4050 50  0000 C CNN
F 1 "10K" V 4500 4050 50  0000 C CNN
F 2 "Resistors_SMD:R_0402" V 4430 4050 50  0001 C CNN
F 3 "" H 4500 4050 50  0001 C CNN
	1    4500 4050
	1    0    0    -1  
$EndComp
$Comp
L R R23
U 1 1 5B3D1C09
P 4500 4550
F 0 "R23" V 4580 4550 50  0000 C CNN
F 1 "SPR" V 4500 4550 50  0000 C CNN
F 2 "Resistors_SMD:R_0402" V 4430 4550 50  0001 C CNN
F 3 "" H 4500 4550 50  0001 C CNN
	1    4500 4550
	1    0    0    -1  
$EndComp
$Comp
L VDDA #PWR015
U 1 1 5B3D1EBD
P 3900 3700
F 0 "#PWR015" H 3900 3550 50  0001 C CNN
F 1 "VDDA" H 3900 3850 50  0000 C CNN
F 2 "" H 3900 3700 50  0001 C CNN
F 3 "" H 3900 3700 50  0001 C CNN
	1    3900 3700
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR016
U 1 1 5B3D212E
P 4500 4800
F 0 "#PWR016" H 4500 4550 50  0001 C CNN
F 1 "GND" H 4500 4650 50  0000 C CNN
F 2 "" H 4500 4800 50  0001 C CNN
F 3 "" H 4500 4800 50  0001 C CNN
	1    4500 4800
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR017
U 1 1 5B3D21E1
P 2900 3200
F 0 "#PWR017" H 2900 2950 50  0001 C CNN
F 1 "GND" H 2900 3050 50  0000 C CNN
F 2 "" H 2900 3200 50  0001 C CNN
F 3 "" H 2900 3200 50  0001 C CNN
	1    2900 3200
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR018
U 1 1 5B3D2387
P 5500 3200
F 0 "#PWR018" H 5500 2950 50  0001 C CNN
F 1 "GND" H 5500 3050 50  0000 C CNN
F 2 "" H 5500 3200 50  0001 C CNN
F 3 "" H 5500 3200 50  0001 C CNN
	1    5500 3200
	1    0    0    -1  
$EndComp
$Comp
L C C16
U 1 1 5B3D2824
P 8450 2650
F 0 "C16" H 8475 2750 50  0000 L CNN
F 1 "0.1u" H 8475 2550 50  0000 L CNN
F 2 "Capacitors_SMD:C_0402" H 8488 2500 50  0001 C CNN
F 3 "" H 8450 2650 50  0001 C CNN
F 4 "1276-1043-1-ND" H 8450 2650 60  0001 C CNN "Part No."
	1    8450 2650
	1    0    0    -1  
$EndComp
$Comp
L C C17
U 1 1 5B3D286F
P 8700 2650
F 0 "C17" H 8725 2750 50  0000 L CNN
F 1 "1u" H 8725 2550 50  0000 L CNN
F 2 "Capacitors_SMD:C_0603" H 8738 2500 50  0001 C CNN
F 3 "" H 8700 2650 50  0001 C CNN
F 4 "1276-1182-1-ND" H 8700 2650 60  0001 C CNN "Part No."
	1    8700 2650
	1    0    0    -1  
$EndComp
Text Notes 9550 2700 0    60   ~ 0
15mA\n50mW\nRC=1.47ms
$Comp
L GND #PWR019
U 1 1 5B3D2E2E
P 8450 3000
F 0 "#PWR019" H 8450 2750 50  0001 C CNN
F 1 "GND" H 8450 2850 50  0000 C CNN
F 2 "" H 8450 3000 50  0001 C CNN
F 3 "" H 8450 3000 50  0001 C CNN
	1    8450 3000
	1    0    0    -1  
$EndComp
$Comp
L R R16
U 1 1 5B3D350D
P 6400 2450
F 0 "R16" V 6480 2450 50  0000 C CNN
F 1 "SPR" V 6400 2450 50  0000 C CNN
F 2 "Resistors_SMD:R_0402" V 6330 2450 50  0001 C CNN
F 3 "" H 6400 2450 50  0001 C CNN
	1    6400 2450
	1    0    0    -1  
$EndComp
$Comp
L R R19
U 1 1 5B3D35A4
P 6400 3050
F 0 "R19" V 6480 3050 50  0000 C CNN
F 1 "SPR" V 6400 3050 50  0000 C CNN
F 2 "Resistors_SMD:R_0402" V 6330 3050 50  0001 C CNN
F 3 "" H 6400 3050 50  0001 C CNN
	1    6400 3050
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR020
U 1 1 5B3D365C
P 6400 3300
F 0 "#PWR020" H 6400 3050 50  0001 C CNN
F 1 "GND" H 6400 3150 50  0000 C CNN
F 2 "" H 6400 3300 50  0001 C CNN
F 3 "" H 6400 3300 50  0001 C CNN
	1    6400 3300
	1    0    0    -1  
$EndComp
$Comp
L VDD #PWR021
U 1 1 5B3D3EA7
P 9300 2100
F 0 "#PWR021" H 9300 1950 50  0001 C CNN
F 1 "VDD" H 9300 2250 50  0000 C CNN
F 2 "" H 9300 2100 50  0001 C CNN
F 3 "" H 9300 2100 50  0001 C CNN
	1    9300 2100
	1    0    0    -1  
$EndComp
Wire Notes Line
	4300 1150 4300 950 
Wire Notes Line
	4200 1450 4200 850 
Wire Notes Line
	2600 1050 2600 1300
Wire Notes Line
	2700 1300 2700 1600
Connection ~ 9300 2200
Connection ~ 6400 2700
Wire Wire Line
	6600 2400 6800 2400
Wire Wire Line
	6600 2700 6600 2400
Wire Wire Line
	6400 2700 6600 2700
Connection ~ 6400 2800
Wire Wire Line
	6400 2600 6400 2900
Wire Wire Line
	6400 2800 5900 2800
Connection ~ 6400 2200
Wire Wire Line
	6400 2300 6400 2200
Wire Wire Line
	6400 3300 6400 3200
Connection ~ 8700 2900
Wire Wire Line
	8700 2800 8700 2900
Connection ~ 8950 2900
Wire Wire Line
	8950 2800 8950 2900
Connection ~ 8450 2900
Wire Wire Line
	9300 2900 9300 2800
Wire Wire Line
	8450 2900 9300 2900
Connection ~ 8450 2400
Wire Wire Line
	7800 2400 8450 2400
Wire Wire Line
	8450 2800 8450 3000
Connection ~ 8450 2200
Wire Wire Line
	8450 2200 8450 2500
Connection ~ 8700 2200
Wire Wire Line
	8700 2500 8700 2200
Connection ~ 8950 2200
Wire Wire Line
	8950 2500 8950 2200
Wire Wire Line
	9300 2100 9300 2500
Wire Wire Line
	8300 2200 9300 2200
Wire Wire Line
	7800 2200 8000 2200
Wire Wire Line
	5500 3200 5500 3100
Connection ~ 2900 2700
Wire Wire Line
	4800 2800 5100 2800
Wire Wire Line
	4800 3500 4800 2800
Wire Wire Line
	4200 3500 4800 3500
Wire Wire Line
	4200 2700 4200 3500
Wire Wire Line
	2900 2700 4200 2700
Wire Wire Line
	2900 2600 2900 2800
Wire Wire Line
	2900 3100 2900 3200
Connection ~ 2900 2200
Wire Wire Line
	2900 2200 2900 2300
Wire Wire Line
	4500 4800 4500 4700
Connection ~ 4500 4300
Wire Wire Line
	5000 2900 5100 2900
Wire Wire Line
	5000 4300 5000 2900
Wire Wire Line
	4500 4300 5000 4300
Wire Wire Line
	4500 4200 4500 4400
Connection ~ 3900 3800
Wire Wire Line
	3900 3700 3900 3800
Connection ~ 3800 3800
Wire Wire Line
	4500 3800 4500 3900
Wire Wire Line
	4500 2600 4500 2750
Wire Wire Line
	4500 3150 4500 3050
Wire Wire Line
	4500 2700 5100 2700
Connection ~ 1900 2200
Connection ~ 3550 3800
Wire Wire Line
	3550 4000 3550 3800
Wire Wire Line
	3800 3800 3800 4000
Wire Wire Line
	3400 3800 4500 3800
Connection ~ 3550 4500
Wire Wire Line
	3550 4300 3550 4500
Connection ~ 2900 4500
Wire Wire Line
	3800 4500 3800 4300
Connection ~ 1900 4500
Wire Wire Line
	1900 4500 3800 4500
Wire Wire Line
	2900 4300 2900 4500
Connection ~ 1300 2700
Wire Wire Line
	1300 2600 1300 2700
Connection ~ 1000 2700
Wire Wire Line
	1600 2700 1600 2600
Wire Wire Line
	1000 2700 1600 2700
Wire Wire Line
	1000 2600 1000 2800
Connection ~ 1600 2200
Wire Wire Line
	1600 2300 1600 2200
Connection ~ 1300 2200
Wire Wire Line
	1300 2300 1300 2200
Connection ~ 1000 2200
Wire Wire Line
	1000 2200 6800 2200
Wire Wire Line
	1000 2100 1000 2300
Connection ~ 1900 3500
Wire Wire Line
	2100 3800 2400 3800
Wire Wire Line
	2100 3500 2100 3800
Wire Wire Line
	1900 3500 2100 3500
Wire Wire Line
	1900 2200 1900 3600
Connection ~ 1900 4000
Wire Wire Line
	1900 3900 1900 4100
Wire Wire Line
	2400 4000 1900 4000
Wire Notes Line
	4300 1700 4800 1700
Wire Notes Line
	4200 1500 4300 1700
Wire Notes Line
	4100 1500 4200 1500
Wire Notes Line
	4400 1400 4800 1400
Wire Notes Line
	4300 1200 4400 1400
Wire Notes Line
	4100 1200 4300 1200
Wire Notes Line
	4500 1100 4800 1100
Wire Notes Line
	4200 800  4500 1100
Wire Notes Line
	4100 800  4200 800 
Wire Notes Line
	2800 1500 3100 1500
Wire Notes Line
	2700 1700 2800 1500
Wire Notes Line
	2400 1700 2700 1700
Wire Notes Line
	2700 1200 3100 1200
Wire Notes Line
	2600 1400 2700 1200
Wire Notes Line
	2400 1400 2600 1400
Wire Notes Line
	2800 800  3100 800 
Wire Notes Line
	2500 1100 2800 800 
Wire Notes Line
	2400 1100 2500 1100
Wire Wire Line
	1900 4400 1900 4600
$Comp
L GND #PWR022
U 1 1 5B3D4CA8
P 7300 2800
F 0 "#PWR022" H 7300 2550 50  0001 C CNN
F 1 "GND" H 7300 2650 50  0000 C CNN
F 2 "" H 7300 2800 50  0001 C CNN
F 3 "" H 7300 2800 50  0001 C CNN
	1    7300 2800
	1    0    0    -1  
$EndComp
Wire Wire Line
	7300 2800 7300 2700
Text Notes 3100 2700 0    60   ~ 0
Vth=4.3V
$Comp
L VDDA #PWR023
U 1 1 5B3D5A35
P 4500 2600
F 0 "#PWR023" H 4500 2450 50  0001 C CNN
F 1 "VDDA" H 4500 2750 50  0000 C CNN
F 2 "" H 4500 2600 50  0001 C CNN
F 3 "" H 4500 2600 50  0001 C CNN
	1    4500 2600
	1    0    0    -1  
$EndComp
Connection ~ 4500 2700
Text Label 3600 2700 0    60   ~ 0
VDD_EN_SENSE
Text Label 4550 4300 0    60   ~ 0
VDD_EN_MR
Text Label 5950 2800 0    60   ~ 0
~VDD_SHDN
Text Label 7850 2200 0    60   ~ 0
VDD_L
Text Label 2000 4000 0    60   ~ 0
VDDA_EN
$Comp
L PWR_FLAG #FLG024
U 1 1 5B41D3F3
P 8800 2100
F 0 "#FLG024" H 8800 2175 50  0001 C CNN
F 1 "PWR_FLAG" H 8800 2250 50  0000 C CNN
F 2 "" H 8800 2100 50  0001 C CNN
F 3 "" H 8800 2100 50  0001 C CNN
	1    8800 2100
	1    0    0    -1  
$EndComp
Wire Wire Line
	8800 2100 8800 2200
Connection ~ 8800 2200
$EndSCHEMATC
