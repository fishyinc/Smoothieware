/*
      This file is part of Smoothie (http://smoothieware.org/). The motion control part is heavily based on Grbl (https://github.com/simen/grbl) with additions from Sungeun K. Jeon (https://github.com/chamnit/grbl)
      Smoothie is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
      Smoothie is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
      You should have received a copy of the GNU General Public License along with Smoothie. If not, see <http://www.gnu.org/licenses/>.
*/

using namespace std;
#include <vector>
#include "libs/nuts_bolts.h"
#include "libs/RingBuffer.h"
#include "../communication/utils/Gcode.h"
#include "libs/Module.h"
#include "libs/Kernel.h"
#include "Timer.h" // mbed.h lib
#include "wait_api.h" // mbed.h lib
#include "Block.h"
#include "Player.h"
#include "Planner.h"

Player::Player(){
    this->current_block = NULL;
    this->looking_for_new_block = false;
    flush_blocks = 0;
}

void Player::on_module_loaded()
{
    register_for_event(ON_IDLE);
}

void Player::on_idle(void* argument)
{
    if (flush_blocks && queue.size())
    {
        Block* block = queue.get_ref(0);
        while (block->gcodes.size())
        {
            Gcode* gcode = block->gcodes.back();
            block->gcodes.pop_back();
            gcode->queued--;
            if (gcode->queued == 0)
                delete gcode;
        }
        queue.delete_first();

        __disable_irq();
        flush_blocks--;
        __enable_irq();
    }
}

// Append a block to the list
Block* Player::new_block(){

    // Clean up the vector of commands in the block we are about to replace
    // It is quite strange to do this here, we really should do it inside Block->pop_and_execute_gcode
    // but that function is called inside an interrupt and thus can break everything if the interrupt was trigerred during a memory access

    // Take the next untaken block on the queue ( the one after the last one )
    Block* block = queue.next_head();
    block->clean();

    block->initial_rate = -2;
    block->final_rate = -2;
    block->player = this;

    queue.consume_head();

    return block;
}

// Used by blocks to signal when they are ready to be used by the system
void Player::new_block_added(){
    if( this->current_block == NULL ){
        this->pop_and_process_new_block(33);
    }
}

// Process a new block in the queue
void Player::pop_and_process_new_block(int debug){
    if( this->looking_for_new_block ){ return; }
    this->looking_for_new_block = true;

    if( this->current_block != NULL ){ this->looking_for_new_block = false; return; }

    // Return if queue is empty
    if( this->queue.size() <= flush_blocks ){
        this->current_block = NULL;
        // TODO : ON_QUEUE_EMPTY event
        this->looking_for_new_block = false;
        return;
    }

    // Get a new block
    this->current_block = this->queue.get_ref(flush_blocks);

    this->current_block->state = 1;

    // Tell all modules about it
    this->kernel->call_event(ON_BLOCK_BEGIN, this->current_block);

    // In case the module was not taken
    if( this->current_block->times_taken < 1 ){
        this->current_block->release();
    }

    this->looking_for_new_block = false;

}

void Player::wait_for_queue(int free_blocks)
{
    while( this->queue.size() >= this->queue.capacity()-free_blocks ){
        this->kernel->call_event(ON_IDLE);
    }
}
