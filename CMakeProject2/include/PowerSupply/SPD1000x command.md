3.4 Command description
1. *IDN?
Command format *IDN?
Description Query the manufacturer, product type, series NO. , software version and
hardware version.
Return Info Manufacturer, product type, series NO., software version.
Example Siglent, SPD1168X, SPD1XDAD1R0001, 2.01.01.06, V1.0
2. *SAV
Command format *SAV <name>
Description Save current state in nonvolatile memory with the specified name.
Example *SAV 1
3. *RCL
Command format *RCL <name>
Description Recall state that had been saved from nonvolatile memory.
Example *RCL 1
4. *DEL
Command format *DEL <name>
Description Delete state that had been saved from nonvolatile memory.
Example *DEL 1
SPD1000X User Manual
int.siglent.com 55
5. INSTrument
Command format INSTrument <CH1>
Description Select the channel that will be operated.
Example INSTrument CH1
Command format INSTrument?
Description Query the current operating channel
Example INSTrument?
Return Info CH1
6. MEASure
Command format MEASure:CURRent? < CH1>
Description Query current value for specified channel, if there is no specified channel,
query the current channel.
Example MEASure:CURRent? CH1
Return Info 3.000
Command format MEASure:VOLTage? < CH1>
Description Query voltage value for specified channel, if there is no specified channel,
query the current channel.
Example MEASure:VOLTage? CH1
Return Info 16.000
Command format MEASure:POWEr? < CH1>
Description Query power value for specified channel, if there is no specified channel,
SPD1000X User Manual
56 int.siglent.com
query the current channel.
Example MEASure:POWEr? CH1
Return Info 90.000
7. CURRent
Command format <SOURce:>CURRent <value>
<SOURce>:={CH1}
Description Set current value of the selected channel
Example CH1:CURRent 0.5
Command format <SOURce>:CURRent?
<SOURce>:={CH1}
Description Query the current value of the selected channel.
Example CH1:CURRent?
Return Info 0.500
8. VOLTage
Command format <SOURce>:VOLTage <value>
<SOURce>:={CH1}
Description Set voltage value of the selected channel
Example CH1:VOLTage 15
Command format <SOURce>:CURRent?
<SOURce>:={CH1}
Description Query the voltage value of the selected channel.
SPD1000X User Manual
int.siglent.com 57
Example CH1:VOLTage?
Return Info 15.000
9. OVP
Command format OVP <value>
Description Set the voltage protection value.
Example OVP 16
Command format OVP?
Description Query the voltage protection value.
Example OVP?
Return Info 16.000
10. OCP
Command format OCP <value>
Description Set the current protection value.
Example OCP 5
Command format OCP?
Description Query the current protection value.
Example OCP?
Return Info 5.000
11. MODE
SPD1000X User Manual
58 int.siglent.com
Command MODE:SET {2W|4W}
Description To set the work operation of 2W or 4W
Example MODE:SET 4W
12. OUTPut
Command format OUTPut <SOURce>, <state>
<SOURce>:={CH1}; <state>:={ON|OFF}
Description Turn on/off the channel.
Example OUTPut CH1,ON
Command format OUTPut:WAVE <SOURce>, <state>
<SOURce>:={CH1}; <state>:={ON|OFF}
Description Turn on/off the waveform display of the channel.
Example OUTPut:WAVE CH1,ON
Command format OUTPut:RESEt:PROTect
Description Clear the overvoltage / overcurrent protection pop-up window.
Example OUTPut:RESEt:PROTect
13. TIMEr
Command format TIMEr:SET <SOURce>, <secnum>, <volt>, <curr>, <time>
<SOURce>:={CH1}; < secnum >;=1 to 5;
Description Set timing parameters of specified channel
Example TIMEr:SET CH1, 2, 3, 0.5, 2
SPD1000X User Manual
int.siglent.com 59
Command format TIMEr:SET? <SOURce>, <secnum>
<SOURce>:={CH1}; < secnum >;=1 to 5;
Description Query the voltage/current/time parameters of specified group of specified
channel.
Example TIMEr:SET? CH1,2
Return Info 3, 0.5, 2
Command format TIMEr <SOURce>, <state>
<SOURce>:={CH1}; < state >;={ON | OFF};
Description Turn on/off Timer function of specified channel
Instruction The command works effectively only when <secnum> starts from 1.
Example TIMEr CH1,ON
14. SYSTem
Command format SYSTem:ERRor?
Description Query the error code and the information of the equipment.
Command format SYSTem:VERSion?
Description Query the software version of the equipment.
Example SYSTem:VERSion?
Return Info 2.01.01.06
Command format SYSTem:STATus?
Description Query the current working state of the equipment.
Instruction The return info is Hexadecimal format, but the actual state is binary, so you
must change the return info into a binary format. The state correspondence
SPD1000X User Manual
60 int.siglent.com
relationship is as follows.
Example SYSTem:STATus?
Return info 0x0224
Explanation: The returned information is hexadecimal, so the user needs to convert to binary format
when confirming the status. See the following table:
Bit NO. Corresponding state
0 0: CV mode; 1: CC mode
4 0: Output OFF; 1: Output ON
5 0: 2W mode; 1: 4W mode
6 0: TIMER OFF; 1: TIMER ON
8 0: digital display; 1: waveform display
15. IPaddr
Command format IPaddr <IP address>
Description Used to assign a Static Internet Protocol (IP) address to the instrument
Example IPaddr 10.11.13.214
Explanation This command is invalid when the power is currently set to automatically
obtain the network configuration (DHCP is ON)
Command format IPaddr?
Description Query the software the setting of IP address
Example SYSTem:VERSion?
Return Info 10.11.13.214
SPD1000X User Manual
int.siglent.com 61
16. MASKaddr
Command format MASKaddr <NetMask>
Description Used to assign a subnet mask to the instrument
Example MASKaddr 255.255.255.0
Explanation This command is invalid when the power is currently set to automatically
obtain the network configuration (DHCP is ON)
Command format MASKaddr?
Description Query the software the setting of mask address
Example SYSTem:VERSion?
Return Info 255.255.255.0
17. GATEaddr
Command format GATEaddr <GateWay>
Description Used to assign agateway to the instrument
Example GATEaddr 10.11.13.1
Explanation This command is invalid when the power is currently set to automatically
obtain the network configuration (DHCP is ON)
Command format MASKaddr?
Description Query the software the setting of gateway address
Return Info 10.11.13.1
18. DHCP
SPD1000X User Manual
62 int.siglent.com
Command format DHCP {ON|OFF}
Description Turn on or off the instrument's automatic network configuration feature.
Example DHCP ON
Command format DHCP?
Description This is used to query whether the current automatic network configuration
of the instrument is enabled
Return Info ON
SPD1000X User Manual
int.siglent.com 63
19. *LOCK
Command format *LOCK
Description Turn on the key lock to disable local or remote settings.
Example *LOCK
Command format *UNLOCK
Description Turn off the key lock to validate the setting
Example *UNLOCK
SPD1000X User Manual
64 int.siglent.com
3.5 Programming examples
This section lists examples of programming with SCPI commands based on NI-VISA or Socket in
Visual C ++, Visual Basic, MATLAB, Python, and more.