#include "libs/Module.h"
#include "libs/Kernel.h"
#include "modules/communication/utils/Gcode.h"
#include "modules/robot/Stepper.h"
#include "Lamp.h"
#include "libs/nuts_bolts.h"
#include "Config.h"
#include "StreamOutputPool.h"
#include "Block.h"
#include "checksumm.h"
#include "ConfigValue.h"
#include "libs/Pin.h"
#include "Gcode.h"

#define lamp_module_enable_checksum		CHECKSUM("lamp_module_enable")
#define lamp_module_pin_checksum		CHECKSUM("lamp_module_pin")

Lamp::Lamp(){}

void Lamp::on_module_loaded() {

	// heater copy "check if module loaded"
	if( !THEKERNEL
		->config
		->value(lamp_module_enable_checksum)
		->by_default(false)
		->as_bool()
	){
		delete this;
		return;
	}
	
	// heater copy "Get pin variable from config"
	this->heater_pin.from_string(
		THEKERNEL
		->config
		->value(lamp_module_pin_checksum)
		->by_default ("nc")
		->as_string()
	)->as_output();

	if (!this->heater_pin.connected()){
		THEKERNEL->streams->printf("Error using this pin number. Lamp module disabled.\n");
		delete this;
		return;
	}

	// Register for kernel events
	this->register_for_event(ON_GCODE_EXECUTE);
	this->register_for_event(ON_PLAY);
	this->register_for_event(ON_PAUSE);
	this->register_for_event(ON_BLOCK_BEGIN);
	this->register_for_event(ON_BLOCK_END);
	
}

// Turn off the heater at the end of blocks
void Lamp::on_block_end(void* argument){

	this->heater_pin.set(false);

}

// Turn on the heater if needed at the beginning of a block
void Lamp::on_block_begin(void* argument){

	if(this->heater_on && THEKERNEL->stepper->get_current_block()){
		this->heater_pin.set(true);
	} else {
		this->heater_pin.set(false);
	}

}


// Turn off the valve on pause
void Lamp::on_pause(void* argument){

	this->heater_pin.set(false); 
}

// Turn on the heater if needed on the play event
void Lamp::on_play(void* argument){

	if(this->heater_on && THEKERNEL->stepper->get_current_block()){
		this->heater_pin.set(true);
	} else {
		this->heater_pin.set(false);
	}

}

// Turn heater on / off depending on the G Code supplied
void Lamp::on_gcode_execute(void* argument){

	Gcode* gcode = static_cast<Gcode*>(argument);
	this->heater_on = false;
	if( gcode->has_g){
		int code = gcode->g;
		if( code != 4 ){
			// heater off
			this->heater_pin.set(false);
			this->heater_on = false;
		}else if (code == 4){
			// heater on, at G4 wait code
			this->heater_on = true;
		}
	}

} 
\ No newline at end of file
