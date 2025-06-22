AUTOEXEC:

	system_ready = 0
	!CALL _digitalOut
	!CALL _digitalIn
	CALL _DEFAULT
	!CALL STARTUP		! power up auto  homing n PTP move
STOP
	
! Startup routine	
STARTUP:	

	system_ready = 0
	CALL _DEFAULT
	
	START 2, HOMEALL
	DISP "Start Homing"
	TILL  PST(2).#RUN=0
	
	! Restore standard motion parameter values
	!CALL _DEFAULT
	
	DISP "Homing done. System ready."
	system_ready = 1
	!START 3, POINTMOVE
STOP



! Subroutine for definition of the default parameters
_DEFAULT:
	
	! Home Position Offset 
	homeOffset(0) = 0
	homeOffset(1) = 0
	homeOffset(2) = 0
!	homeOffset(1) = 15

	
	
	! Homing Methods, 1: Negative limit + index, 17: Negative limit, 34: Positive Index, 0: no homing (for absolute encoder, only move to home position)
	HOMEDEF(0) = 17
	HOMEDEF(1) = 17
	HOMEDEF(2) = 17
!	HOMEDEF(1) = 50

	
	! Max. Current during homing
	homeCurrent(0) = XRMS(0)/2
	homeCurrent(1) = XRMS(1)/2
	homeCurrent(2) = XRMS(1)/2
!	homeCurrent(1) = XRMS(1)


	! Standard velocity
	std_velocity(0) = 50
	std_velocity(1) = 50
	std_velocity(2) = 40
!	std_velocity(1) = 1000


	! Max. velocity
	XVEL(0) = 100
	XVEL(1) = 100
	XVEL(2) = 80
!	XVEL(1) = 5000


		
	! Acceleration	
	std_acceleration(0) = std_velocity(0)*10
	std_acceleration(1) = std_velocity(1)*10
	std_acceleration(2) = std_velocity(2)*10
!	std_acceleration(1) = std_velocity(1)*10


	! Deceleration	
	std_deceleration(0) = std_acceleration(0)
	std_deceleration(1) = std_acceleration(1)
	std_deceleration(2) = std_acceleration(2)
!	std_deceleration(1) = std_acceleration(1)


	! Kill deceleration
	std_kdeceleration(0) = std_acceleration(0)*2
	std_kdeceleration(1) = std_acceleration(1)*2
	std_kdeceleration(2) = std_acceleration(2)*2
!	std_kdeceleration(1) = std_acceleration(1)*2

	
	! Jerk
	std_jerk(0)	= std_acceleration(0)*10
	std_jerk(1)	= std_acceleration(1)*10
	std_jerk(2)	= std_acceleration(2)*10
!	std_jerk(1) = std_acceleration(1)*10	

	
	! Homing velocity
	homeVel(0) = 5
	homeVel(1) = 5
	homeVel(2) = 5
!	homeVel(1) = 30

	
	! Homing Acceleration & Deceleration
	homeAccDec(0) = std_deceleration(0)
	homeAccDec(1) = std_deceleration(1)
	homeAccDec(2) = std_deceleration(2)
!	homeAccDec(1) = std_deceleration(1)

	
	! SoftLimits
	!softLimitPos(0) = 25.1
	!softLimitNeg(0) = -0.1
!	softLimitPos(1) = 405
!	softLimitNeg(1) = -5

	! Axis is stepper
	isStepper(0) = 1
	isStepper(1) = 1
	isStepper(2) = 1
	isStepper(4) = 1
	isStepper(5) = 1
	isStepper(6) = 1

	
	! Assign parameters to axes
	VEL(0) = std_velocity(0)
	ACC(0) = std_acceleration(0)
	DEC(0) = std_deceleration(0)
	KDEC(0) = std_kdeceleration(0)
	JERK(0) = std_jerk(0)
	
	VEL(1) = std_velocity(1)
	ACC(1) = std_acceleration(1)
	DEC(1) = std_deceleration(1)
	KDEC(1) = std_kdeceleration(1)
	JERK(1) = std_jerk(1)	
	
	VEL(2) = std_velocity(2)
	ACC(2) = std_acceleration(2)
	DEC(2) = std_deceleration(2)
	KDEC(2) = std_kdeceleration(2)
	JERK(2) = std_jerk(2)	
RET
	
!RET

! Definition of digital outputs
_digitalOut:
	
	!SETCONF(29,AANN00) AA: Axis, NN: digital output index, 00: 0-digital output, 2-brake
	!SETCONF(29,010009,0) ! Brake Axis 1 to Device 0 OUT 9
	!SETCONF(29,100400,2) ! Digital Output 4, Axis 10 as Brake
	
RET

! Definition of MARK pins as digital inputs
_digitalIn:

	!ASSIGNFINS axis, output_index, bit_code (0: IN_0, 1: IN_1)
	!ASSIGNFINS 0, 0, 0b1111 ! MARK 0 to Device 0, IN 16