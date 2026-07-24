
	INCLUDE "hardware/custom.i"
	INCLUDE "hardware/cia.i"
	INCLUDE "devices/input.i"
	INCLUDE "devices/inputevent.i"
	INCLUDE "resources/potgo.i"
	INCLUDE "lvo/exec_lib.i"
	INCLUDE "lvo/potgo_lib.i"

md_head			 	equ 0
md_tail			 	equ 1
md_sigbit		 	equ 2
md_Task			 	equ 6
md_codes		 	equ 14

_custom          	equ $DFF000
_ciaa 			 	equ $BFE001

; Port 0 (For actual mouse with AmiBLEHID)
; -------------------------------------------------
ciab_gameport       equ 6						; fire button bit in ciapra
pot_bit             equ 10						; data bit to read
potgo_set           equ $0200					
potgo_clear         equ $0300

; Port 1 (For testing in WinUAE with a controller)
; -------------------------------------------------
;ciab_gameport       equ 7
;pot_bit             equ 14
;potgo_set           equ $2000
;potgo_clear         equ $3000


	* Entered with:       A0 == scratch (execpt for highest pri vbl server)
	*  D0 == scratch      A1 == is_Data
	*  D1 == scratch      A5 == vector to interrupt code (scratch)
	*                     A6 == scratch
	*
	
	SECTION CODE	
	
	XDEF    _VertBServer

_VertBServer:
																				
	MOVE.b md_tail(A1),D0					; 256 entry FIFO. IRQ code is writing to head++
	SUB.b md_head(A1),D0					; driver follows with reading tail++. if (head + 1) == tail, we're full
	CMP.b #1,D0								; check where the head and tail is in FIFO		
	BEQ.s	abort							; if (tail - head) = 1 we're full

	; Read extra buttons, CD32 controller protocol
	; See: https://eab.abime.net/showthread.php?t=29768&page=2	
		
	MOVEM.L     D2/A0-A1,-(SP)				; Preserve regs
	
	LEA     	_custom, a0
	LEA  		_ciaa,   a1	

	BSET		#ciab_gameport,ciaddra(A1)  ; Set pin 6 (clock/fire) to output
	BCLR		#ciab_gameport,ciapra(A1)	; Initial clock state
	MOVE.W		#potgo_set,potgo(A0)        ; Pin 9 (Pot) as output, initiates shift register reading
 	MOVEQ		#0,d0
 	MOVEQ       #0,d1
	BRA.b       cd32_in
	
cd32_loop:
	TST.b       (A1)						; Delay
	TST.b       (A1)
	
cd32_in:
	TST.b       (A1)
	TST.b       (A1)
	TST.b       (A1)
	TST.b       (A1)
	TST.b       (A1)
	TST.b       (A1)
	
	MOVE.W 		potinp(A0),D2				; Read currently shifted button bit;
	BSET		#ciab_gameport,ciapra(A1)	; Clock pulse
	BCLR		#ciab_gameport,ciapra(A1)
	
	BTST		#pot_bit,D2					; Is current button held?
	BNE.b       cd32_next					
	BSET        D1,D0						; Set button bit
	
cd32_next:
    ADD.b       #1,d1
	CMPI.b      #9,d1                       ; We read 9 bits. Final 2 are 0 then 1, this identifies a valid pad read
	BNE.b		cd32_loop				    ; Loop if more buttons to read
		
	BCLR		#ciab_gameport,ciaddra(A1)	; Pin 6 back to input
	MOVE.W      #potgo_clear,potgo(A0)		; Pin 9 back to input

	MOVEM.L     (SP)+,D2/A0-A1				; Restore from stack		
		
	MOVE.W      D0,D1						; Do we have a valid pad read?  
	AND.W       #$180,D1					; If 9th bit is 1 and 8th bit is 0, read is good
	CMP         #$100,D1
	BNE         abort						; Expected bits don't match, don't output anything	
 
	MOVE.W      D0,D1
	
	; Output event and signal main task	
	MOVEQ		#0,D0						; just in case clear entire D0
	MOVE.b		md_head(A1),D0				; get the current head counter
	MOVE.b		D1,md_codes(A1,D0.w)		; place current code at the head position		

	MOVE.L		A1,-(SP)					; preserve A1 in the stack
	MOVE.L  	4.W,A6						; ExecBase
	MOVE.L  	md_sigbit(A1),D0			; is_Data->sigbit
	MOVE.L  	md_Task(A1),A1				; is_Data->task
	JSR _LVOSignal(A6)
	MOVE.L		(SP)+,A1					; restore A1 from the stack
	
	ADD.b		#$1,md_head(A1)				; increment message counter

abort:
	LEA		_custom,A0						; if you install a vertical blank server at priority 10 or greater, you must place custom ($DFF000) in A0 before exiting
	MOVEQ.L #0,D0							; set Z flag to continue to process other vb-servers
	RTS
