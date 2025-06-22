! Author: Jethro
! Date: 16 DEC 2021
! Buffer 2: Homing functions

! Local variable for definition of current axis
int axis

! Function for homing of all axes
HOMEALL:

	! Home axis
	
!	CALL HOME00
!
!	CALL HOME01
!		
!	CALL HOME02

	axis = 0
	CALL _home
	PTP/e(axis),0
	
	axis = 1
	CALL _home
	PTP/e(axis),0
	
	axis = 2
	CALL _home
	PTP/e(axis),0
	
STOP

HOME00:

	! Home axis
	axis = 0
	CALL _home
	PTP/e(axis),0

STOP

HOME01:

	! Home axis

	axis = 1
	CALL _home
	PTP/e(axis),0
	

STOP

HOME02:

	! Home axis

	axis = 2
	CALL _home
	PTP/e(axis),0
STOP


! Measure distance between negative limit switch and first index pulse.
INDPOS:

	! Define axis
	axis = 0
	
	! Homing on Left limit (17), no Offset, neg hard stop+index
	CALL _enable
	HOME axis, 50, homeVel(axis),,, homeCurrent(axis)
	TILL MFLAGS(axis).#HOME = 1
	
	CALL _findIndex
	
	MFLAGS(axis).#HOME = 0

STOP

	
! Subroutine for homing with standard homing methods. Variable axis must be defined before calling this function.
_home:

	! Reset Home flag
	MFLAGS(axis).#HOME = 0
	
	! Enable motor
	CALL _enable
	
	! Set Homing motion parameters
	DEC(axis) = homeAccDec(axis)
	ACC(axis) = homeAccDec(axis)
	JERK(axis) = homeAccDec(axis)*2
	
	
	IF (HOMEDEF(axis) <> 0) ! standard homing methods
		IF (HOMEDEF(axis) <> 40) ! standard homing methods
			HOME axis, HOMEDEF(axis), homeVel(axis), , homeOffset(axis), homeCurrent(axis)
			TILL ^AST(axis).#INHOMING ! wait until homing ready
		ELSEIF (HOMEDEF(axis)=80) !manual jog homing methods
			CALL _manualjog
		else 
			CALL _homeRef
		END
	ELSE ! for absolute encoder axis, only move to home position
				
		PTP axis, homeOffset(axis)
		TILL MST(axis).#INPOS
		MFLAGS(axis).#HOME = 1
		
	
	END
	
	!adjust ref position for gantry 
	IF MFLAGS(axis).#GANTRY = 1
		DISABLE(axis)
		SET FPOS(1) = 0.0
		ENABLE(axis)
	END

	! Check homing flag
	IF MFLAGS(axis).#HOME = 1		
		DISP "Homing axis %d done (FPOS: %f)", axis, FPOS(axis)
	ELSE
		DISP "Homing axis %d not successfull!", axis
	END
	
	! Set soflimits
	!SRLIMIT(axis) = softLimitPos(axis)
	!SLLIMIT(axis) = softLimitNeg(axis)
	
RET


! Subroutine for finding an index. Variable axis must be defined before calling this function.
_findIndex:

	! Reset Index state
	IST(axis).#IND = 1
	IST(axis).#IND = 0
 
	JOG/v axis, homeVel(axis)
	TILL IST(axis).#IND = 1, 30000
	HALT(axis)
	!rangemeasPos(0) = FPOS(axis)
	
	! Check if index found (or timeout)
	IF IST(axis).#IND = 0
		DISP "Could not find Index"
 	ELSE
	
		WAIT 500
		
  		! Read position latched at index
  		indexPos(axis) = IND(axis)
		DISP "Index Position: %3.6f", indexPos(axis)
		DISP "Phase Angle: %3.2f degree", GETCONF(214,axis)
		
	END

RET


! Subroutine for enabling the motor and do commutation if necessary. Variable axis must be defined before calling this function.
_enable:

	! Enable motor
	ENABLE(axis)
	TILL MST(axis).#ENABLED
	
	IF MST(axis).#ENABLED
 		DISP "Enabled motor of axis %d.", axis
		If MFLAGSX(axis).0=0
		
 		ELSEIF MFLAGS(axis).#BRUSHL = 1 & isStepper(axis) = 0 ! Check if motor is brushless
		
			IF MFLAGS(axis).#BRUSHOK = 0  ! Check if motor is commutated
  				
					CALL _doCommut
			
 			END
		END
		
	ELSE
		DISP "ERROR on axis %d. Enable did fail.", axis
	END
	
RET

! Subroutine for commutation of the motor. Variable axis must be defined before calling this function.
_doCommut:

	! Fast commutation. Axis should be tuned properly.
  	COMMUT(axis)
	
  	TILL (MFLAGS(axis).#BRUSHOK = 1), 2000	!timeout after 2s
  	
	! Check commutation
	IF MFLAGS(axis).#BRUSHOK = 0 
    	DISP "ERROR on axis %d. Commutation did fail.", axis
  	ELSE
   		DISP "Phase Angle at actual position: %3.2f degree", (GETCONF(214,axis))
   		DISP "Commutation motor %d OK.", axis
  END
  
RET

!------------------------------------------------------------------------------------------------------------------
! Homing on reference switch. Variable axis must be defined before calling this function.
_homeRef:
	! Deactivate default fault on softlimits
  	FDEF(axis).#SLL = 0  
 	FDEF(axis).#SRL = 0
	
	!IN2.0 = 1 (180-360?) IN2.0 (0-180?)
	IF(IN(0).0 = 1) !axis is between 180-360?
 		JOG/v axis, -homeVel(axis) ! Move in Positive direction
 		TILL (IN(2).0 = 0) ! Until left limit is detected (REF-switch)
		PTP/vr axis, 5, homeVel(axis) ! Move away from limit
 		HALT(axis)
	END
	
	JOG/v axis, homeVel(axis) ! Move in negative direction
 		TILL (IN(0).0 = 1) ! Until left limit is detected (REF-switch)
 		HALT(axis)
 
 		wait 500       
 		JOG/v axis, -homeVel(axis) ! Move out of limit switch (REF-switch)
 		TILL (IN(0).0 = 0)
 		HALT (axis)
		TILL AST(axis).#MOVE = 0
			
		
		WAIT 500
							!ENABLE(axis)
 		SET FPOS(axis) = homeOffset(axis) 	!Position offset of feedback position
	
 	DISP "REF-switch position: %3.6f", (FPOS(axis))
	MFLAGS(axis).#HOME = 1
	
	!activates soft limit detection
	FMASK(axis).#SLL = 1  
 	FMASK(axis).#SRL = 1
	FDEF(axis).#SLL = 1  
 	FDEF(axis).#SRL = 1
	

	
RET

_manualjog:


JOG (axis),-           ! Move to the left limit switch 
TILL FAULT(axis).#LL   ! Wait for the left limit switch activation
  ! Can be written also as "TILL ABS(PE(iAxis))>???" when no limit switches exist - only a hard stop.
JOG (axis),+           ! Move to the encoder index
TILL ^FAULT(axis).#LL  ! Wait for the left limit release
!IST(axis).#IND=0       ! Reset the index flag - activate index circuit
!TILL IST(axis).#IND    ! Wait for crossing the index
SET FPOS(axis)=FPOS(axis)-IND(axis)  ! Set iAxis origin to the position of index = zero
PTP (axis),homeOffset(axis)           ! Move to the origin
FDEF(axis).#LL=1       ! Enable the iAxis left limit default response
FDEF(axis).#RL=1       ! Enable the iAxis right limit default response
             