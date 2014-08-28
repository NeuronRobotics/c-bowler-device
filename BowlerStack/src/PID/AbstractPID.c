/**
 * @file AbstractPID.c
 *
 * Created on: Apr 9, 2010
 * @author Kevin Harrington
 */


#include "Bowler/Bowler.h"

int number_of_pid_groups= 0;

static AbsPID * pidGroupsInternal=NULL;
static PD_VEL * velData=NULL;

float (*getPosition)(int);
void (*setOutputLocal)(int, float);
void setOutput(int group, float val);
int (*resetPosition)(int,int);
void (*onPidConfigureLocal)(int);
void (*MathCalculationPosition)(AbsPID * ,float );
void (*MathCalculationVelocity)(AbsPID * ,float );
PidLimitEvent* (*checkPIDLimitEvents)(uint8_t group);



//static BowlerPacket packetTemp;

void OnPidConfigure(int v){
	onPidConfigureLocal( v);
}

int getNumberOfPidChannels(){
	return number_of_pid_groups;
}

PD_VEL  * getPidVelocityDataTable(int group){
	if(velData == NULL){
		println_E("Velocity data table is null");
		return NULL;
	}
	return &velData[group];
}

AbsPID * getPidGroupDataTable(int group){
	if(pidGroupsInternal == NULL){
		println_E("PID data table is null");
		return NULL;
	}

	return &pidGroupsInternal[group];
}

boolean isPidEnabled(uint8_t i){
    return getPidGroupDataTable(i)->config.Enabled;
}

void SetPIDEnabled(uint8_t index, boolean enabled){

    getPidGroupDataTable(index)->config.Enabled=enabled;
}

void SetControllerMath( void (*math)(AbsPID * ,float )){
	if(math !=0)
		MathCalculationPosition=math;
	else
		MathCalculationPosition=&RunAbstractPIDCalc;
}

void InitilizePidController(AbsPID * groups,PD_VEL * vel,int numberOfGroups,
							float (*getPositionPtr)(int),
							void (*setOutputPtr)(int,float),
							int (*resetPositionPtr)(int,int),
							void (*onPidConfigurePtr)(int),
							PidLimitEvent* (*checkPIDLimitEventsPtr)(uint8_t group)){
	if(groups ==0||
		vel==0||
		getPositionPtr==0||
		setOutputPtr==0||
		resetPositionPtr==0||
		checkPIDLimitEventsPtr==0||
		onPidConfigurePtr==0){
		println("Null pointer exception in PID Configure",ERROR_PRINT);
		while(1);
	}
	pidGroupsInternal = groups;
	number_of_pid_groups = numberOfGroups;
	getPosition=getPositionPtr;
	setOutputLocal=setOutputPtr;
	resetPosition=resetPositionPtr;
	onPidConfigureLocal=onPidConfigurePtr;
	checkPIDLimitEvents=checkPIDLimitEventsPtr;
	velData=vel;
	SetControllerMath(&RunAbstractPIDCalc);
        int i;

        for(i=0;i<numberOfGroups;i++){
            int enabled = getPidGroupDataTable(i)->config.Enabled;
            pidReset(i,0);
            getPidGroupDataTable(i)->config.Enabled = enabled ;
        }
}

void SetPIDCalibrateionState(int group, PidCalibrationType state){
    getPidGroupDataTable(group)->config.calibrationState=state;
    //OnPidConfigure(group);
}

PidCalibrationType GetPIDCalibrateionState(int group){

    return getPidGroupDataTable(group)->config.calibrationState;
}


uint8_t ZeroPID(uint8_t chan){
	//println("Resetting PID channel from zeroPID:",INFO_PRINT);
	pidReset(chan,0);
	return true; 
}

uint8_t ClearPID(uint8_t chan){
	if (chan>=getNumberOfPidChannels())
		return false; 
	getPidGroupDataTable(chan)->config.Enabled=false; 
	return true; 
}

uint8_t SetPIDTimedPointer(AbsPID * conf,float val, float current,float ms){
    	if(ms<.01)
		ms=0;
	//local_groups[chan].config.Enabled=true;
	conf->interpolate.set=val;
	conf->interpolate.setTime=ms;
	conf->interpolate.start=current;
	conf->interpolate.startTime=getMs();
	conf->SetPoint=val;
	//conf->config.Enabled=true;
	InitAbsPIDWithPosition(conf,conf->config.K.P,conf->config.K.I,conf->config.K.D, getMs(),current);
	return true;
}

uint8_t SetPIDTimed(uint8_t chan,float val,float ms){
	//println_I("@#@# PID channel Set chan=");p_int_I(chan);print_I(" setpoint=");p_int_I(val);print_I(" time=");p_fl_I(ms);
	if (chan>=getNumberOfPidChannels())
		return false;
        velData[chan].enabled=false;
        return SetPIDTimedPointer(getPidGroupDataTable(chan),val, GetPIDPosition( chan),ms);
}
uint8_t SetPID(uint8_t chan,float val){
	SetPIDTimed(chan, val,0);
	return true; 
}

int GetPIDPosition(uint8_t chan){
	if (chan>=getNumberOfPidChannels())
		return 0;
	//getPidGroupDataTable(chan)->CurrentState=(int)getPosition(chan);
	return getPidGroupDataTable(chan)->CurrentState;
}


float pidResetNoStop(uint8_t chan,int32_t val){
    AbsPID * data = getPidGroupDataTable(chan);
    //float value = (float)resetPosition(chan,val);
    float current=data->CurrentState;
    float raw =current+data->config.offset;
    float value=(float)val;
    data->config.offset = (raw - value);
    data->CurrentState = raw - data->config.offset;
    println_E("From pidReset Current State: ");p_fl_E(current);
    print_E(" Target value: ");p_fl_E(value);
    print_E(" Offset: ");p_int_E(data->config.offset);
    print_E(" Raw: ");p_int_E(raw);
    float time = getMs();
    data->lastPushedValue=val;
    InitAbsPIDWithPosition(getPidGroupDataTable(chan),data->config.K.P,data->config.K.I,data->config.K.D, time,val );
    velData[chan].lastPosition=val;
    velData[chan].lastTime=time;
    return val;
}

void pidReset(uint8_t chan,int32_t val){
	float value = pidResetNoStop(chan,val);
        AbsPID * data = getPidGroupDataTable(chan);
	data->interpolate.set=value;
	data->interpolate.setTime=0;
	data->interpolate.start=value;
	data->interpolate.startTime=getMs();
	data->SetPoint=value;
        uint8_t enabled=data->config.Enabled;
	data->config.Enabled=true; //Ensures output enabled to stop motors
	data->Output=0.0;
	setOutput(chan,data->Output);
	velData[chan].enabled=enabled;
}


void InitAbsPID(AbsPID * state,float KP,float KI,float KD,float time){
	InitAbsPIDWithPosition(state,KP, KI, KD,time,0);
}

void setPIDConstants(int group,float p,float i,float d){
    getPidGroupDataTable(group)->config.K.P=p;
    getPidGroupDataTable(group)->config.K.I=i;
    getPidGroupDataTable(group)->config.K.D=d;
}

/**
 * RunAbstractPIDCalc
 * @param state A pointer to the AbsPID struct to run the calculations on
 * @param CurrentTime a float of the time it is called in MS for use by the PID calculation
 */
void InitAbsPIDWithPosition(AbsPID * state,float KP,float KI,float KD,float time,float currentPosition){
	state->config.K.P=KP;
	state->config.K.I=KI;
	state->config.K.D=KD;
	//state->integralCircularBufferIndex = 0;
	state->integralTotal = 0.0;
        state->integralSize  = 20.0;
	state->SetPoint = currentPosition;
	state->PreviousError=0;
	state->Output=0.0;
	state->PreviousTime=time;
}



boolean isPIDInterpolating(int index){
    return getPidGroupDataTable(index)->interpolate.setTime != 0;
}

boolean isPIDArrivedAtSetpoint(int index, float plusOrMinus){
    if(getPidGroupDataTable(index)->config.Enabled)
        return bound( getPidGroupDataTable(index)->SetPoint,
                        getPidGroupDataTable(index)->CurrentState,
                        plusOrMinus,
                        plusOrMinus);
    return true; 
}

void RunPIDControl(){
    	int i;
	for (i=0;i<getNumberOfPidChannels();i++){
            getPidGroupDataTable(i)->CurrentState = getPosition(i) - getPidGroupDataTable(i)->config.offset;
            if(getPidGroupDataTable(i)->config.Enabled){
                getPidGroupDataTable(i)->SetPoint = interpolate(&pidGroupsInternal[i].interpolate,getMs());
                MathCalculationPosition(getPidGroupDataTable(i),getMs());
                if(GetPIDCalibrateionState(i)<=CALIBRARTION_DONE){
                    setOutput(i,getPidGroupDataTable(i)->Output);
                }else if(GetPIDCalibrateionState(i) == CALIBRARTION_hysteresis){
                    pidHysterisis( i );
                }else if((GetPIDCalibrateionState(i) == CALIBRARTION_home_down) ||(GetPIDCalibrateionState(i) == CALIBRARTION_home_up)||(GetPIDCalibrateionState(i) == CALIBRARTION_home_velocity)){
                    checkLinkHomingStatus(i);
                }
            }

	}
}

void RunPIDComs(BowlerPacket *Packet,boolean (*pidAsyncCallbackPtr)(BowlerPacket *Packet)){
    int i;
	for (i=0;i<getNumberOfPidChannels();i++){
            pushPIDLimitEvent(Packet,pidAsyncCallbackPtr,checkPIDLimitEvents(i));
	}
	updatePidAsync(Packet,pidAsyncCallbackPtr);
}

void RunPID(BowlerPacket *Packet,boolean (*pidAsyncCallbackPtr)(BowlerPacket *Packet)){
    RunPIDControl();
    RunPIDComs(Packet,pidAsyncCallbackPtr);
}

/**
 * InitAbsPID
 * @param state A pointer to the AbsPID the initialize
 * @param KP the Proportional Constant
 * @param KI the Integral Constant
 * @param KD the Derivative Constant
 * @param time the starting time
 */

void RunAbstractPIDCalc(AbsPID * state,float CurrentTime){
	float error;
	float derivative;

        

	//calculate set error
	error = state->SetPoint- state->CurrentState;

	//remove the value that is INTEGRALSIZE cycles old from the integral calculation to avoid overflow
	//state->integralTotal -= state->IntegralCircularBuffer[state->integralCircularBufferIndex];
	//add the latest value to the integral
	state->integralTotal =  (error*(1.0/state->integralSize)) +
                                (state->integralTotal*((state->integralSize-1.0)/state->integralSize));
	
        //This section clears the integral buffer when the zero is crossed
        if((state->PreviousError>=0 && error<0)||
            (state->PreviousError<0 && error>=0)    ){
            state->integralTotal=0;
        }


        //calculate the derivative
        derivative = (error-state->PreviousError);// / ((CurrentTime-state->PreviousTime));
        state->PreviousError=error;
	 
	//do the PID calculation
	state->Output = (   (state->config.K.P*error) +
                            (state->config.K.D*derivative) +
                            (state->config.K.I*state->integralTotal)
                        );

        if(state->config.Polarity==false)
            state->Output *=-1.0;
	//Store the current time for next iterations previous time
	state->PreviousTime=CurrentTime;

}



void setOutput(int group, float val){
    val*=getPidGroupDataTable(group)->config.tipsScale;
    val += getPidStop(group);
    if(val>getPidStop(group) && val<getUpperPidHistoresis(group))
        val = getUpperPidHistoresis(group);
    if(val<getPidStop(group) && val>getLowerPidHistoresis(group))
        val = getLowerPidHistoresis(group);
    getPidGroupDataTable(group)->OutputSet=val;
    setOutputLocal(group,val);
}

