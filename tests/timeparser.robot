*** Settings ***
Library  SerialLibrary  encoding=ascii
Library    String

*** Variables ***
${com}                                 COM15
${board}                               nRF5340
${baud}                                115200
${DL}                                  4

# Used to set DUT to expect robot data using null character
${ROBOMODE}                            \x00

# Error flags
${flag_time_len}                       -1
${flag_time_syn}                       -2
${flag_time_hou}                       -4
${flag_time_min}                       -8
${flag_time_sec}                       -16

*** Test Cases ***
Connect Serial
	Log To Console                     Connecting to ${com} ${board}
	Add Port                           ${com}  baudrate=115200  encoding=ascii
	Port Should Be Open                ${com}
	Reset Input Buffer
	Reset Output Buffer

	${success} =                       Toggle Robot Mode
	Log To Console                     Got response ${success}

	IF  not ${success}
	    Log To Console                 Failed to set the robot mode on DUT. Please reset the board and try again.
		Fatal Error
    END

Serial Time test Wrong Hour
	Reset Input Buffer
	${timestamp} =                     Convert To String  245959\n
	Write Data                         ${timestamp}
	${read} =                          Read Until  terminator=0A
	${read} =                          Strip String  ${read}
	Log To Console                     Received ${read}
	Should Be Equal As Integers        ${read}  ${flag_time_hou}

Serial Time test Wrong Minute
	Reset Input Buffer
	${timestamp} =                     Convert To String  239959\n
	Write Data                         ${timestamp}
	${read} =                          Read Until  terminator=0A
	${read} =                          Strip String  ${read}
	Log To Console                     Received ${read}
	Should Be Equal As Integers        ${read}  ${flag_time_min}

Serial Time test Wrong Second
	Reset Input Buffer
	${timestamp} =                     Convert To String  235999\n
	Write Data                         ${timestamp}
	${read} =                          Read Until  terminator=0A
	${read} =                          Strip String  ${read}
	Log To Console                     Received ${read}
	Should Be Equal As Integers        ${read}  ${flag_time_sec}

Serial Time test Wrong Len
	Reset Input Buffer
	${timestamp} =                     Convert To String  2359\n
	${hex} =                           Convert To Bytes  ${timestamp}
	Log To Console                     Writing bytes ${hex} which are ${timestamp} in ascii
	Write Data                         ${timestamp}
	${read} =                          Read Until  terminator=0A
	${read} =                          Strip String  ${read}
	Log To Console                     Received ${read}
	Should Be Equal As Integers        ${read}  ${flag_time_len}

Serial Time test Wrong Len 2
	Reset Input Buffer
	${timestamp} =                     Convert To String    10101010\x0a
	Write Data                         ${timestamp}  encoding=ascii
	${read} =                          Read Until  terminator=0A  block=True
	# ${binrep} =                        Convert To Binary  ${read}  base=10
	# Log To Console                     Received ${read} whitch is ${binrep} in binary
	${read} =                          Strip String  ${read}
	${flags} =                         Evaluate  ${flag_time_len}
	Should Be Equal As Integers        ${read}  ${flags}

# This is the minimal required test
Serial Time test Correct time
	Reset Input Buffer
	${timestamp} =                     Convert To String  001000\n
	# Write Data                         ${timestamp}  encoding=ascii
	Write Data                         ${timestamp}  encoding=ascii
	Log To Console                     Writing bytes ${timestamp}
	${read} =                          Read Until  terminator=0A  encoding=ascii
	${read} =                          Strip String  ${read}
	Log To Console                     Received ${read}
	Should Be Equal As Integers        ${read}  600

Disconnect Serial
    ${ret} =                           Toggle Robot Mode
	IF  ${ret} == 1
		Log To Console                 Unsetting robot mode was unsuccessful. You should reset the board.
	ELSE
		Log To Console                 Unsetting robot mode was successful.
	END
	Log To Console  Disconnecting ${board}
	[TearDown]  Delete Port  ${com}

*** Keywords ***
# Sets the DUT to expect robot encoded data. This will return either 0 or 1
Toggle Robot Mode
    Log To Console                     Toggling robot mode on ${board}
    Write Data                         ${ROBOMODE}
	${read} =                          Read Until  terminator=0A
	${read} =                          Strip String  ${read}
    Log To Console                     Response value is ${read}
	${ret} =                           Evaluate  ${read} == 1
	RETURN                             ${ret}
