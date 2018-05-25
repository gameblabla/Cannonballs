/***************************************************************************
    Ferrari AI and Logic Routines.
    Used by Attract Mode and the end of game Bonus Sequence. 

    This code contains a port of the original AI and a new enhanced AI.

    Enhanced AI Bug-Fixes:
    ----------------------
    - AI is much better at driving tracks without crashing into scenery.
    - No weird juddering when turning corners.
    - No brake light flickering.
    - Can drive any stage in the game competently. 
    - Selects a true random route, rather than a pre-defined route. 
    - Can handle split tracks correctly.

    It still occasionally collides with scenery on the later stages, but
    I think this is ok - as we want to demo a few collisions!

    Notes on the original AI:
    -------------------------
    The final behaviour of the AI differs from the original game.
    
    This is because the core Ferrari logic the AI relies on is in turn
    dependent on timing behaviour between the two 68k CPUs.
    
    Differences in timing lead to subtle differences in the road x position 
    at that particular point of time. Over time, these differences are 
    magnified. 
    
    MAME does not accurately reproduce the arcade machine with regard to
    this. And the Saturn conversion also exhibits different behaviour.
    
    The differences are relatively minor but noticeable.
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <cstdlib> 
#include <time.h>

#include "engine/oattractai.hpp"
#include "engine/oferrari.hpp"
#include "engine/oinputs.hpp"
#include "engine/ostats.hpp"
#include "engine/otraffic.hpp"

OAttractAI oattractai;

OAttractAI::OAttractAI(void)
{
    srand((unsigned int) time(NULL));
}


OAttractAI::~OAttractAI(void)
{
}

void OAttractAI::init()
{
    last_stage = -1;
}

// ------------------------------------------------------------------------------------------------
//                                           ENHANCED AI CODE
// ------------------------------------------------------------------------------------------------

void OAttractAI::tick_ai_enhanced()
{
    // --------------------------------------------------------------------------------------------
    // Choose Route At Random
    // --------------------------------------------------------------------------------------------

    if (last_stage != ostats.cur_stage)
    {     
        last_stage           = ostats.cur_stage;
        oferrari.sprite_ai_x = std::rand() & 1;     
    }

    // --------------------------------------------------------------------------------------------
    // Steering
    // --------------------------------------------------------------------------------------------
    int16_t future_x; 
    
    // Select road at road split
    if (oinitengine.rd_split_state > 0 && oinitengine.rd_split_state < 8)
        future_x = oferrari.sprite_ai_x ? oroad.road0_h[0x40] : oroad.road1_h[0x40];
    // Roads swap position at merge
    else if (oinitengine.rd_split_state == 9 || oinitengine.rd_split_state == 10)
        future_x = oferrari.sprite_ai_x ? oroad.road1_h[0x40] : oroad.road0_h[0x40];
    // Otherwise just use a standard x-horizon point
    else
        future_x = oroad.road0_h[0x40];

    oferrari.sprite_ai_steer = (future_x * 3);

    if (oferrari.sprite_ai_steer > 0x7F)
        oferrari.sprite_ai_steer = 0x7F;
    else if (oferrari.sprite_ai_steer < -0x7F)
        oferrari.sprite_ai_steer = -0x7F;

    oinputs.steering_adjust   = oferrari.sprite_ai_steer;

    // --------------------------------------------------------------------------------------------
    // Braking: Brake when we get too close to the edge of the track
    // --------------------------------------------------------------------------------------------
    oinputs.brake_adjust = 0;
    if (oinitengine.car_increment >> 16 >= 0x80)
    {
        int16_t x = oinitengine.car_x_pos;
        uint16_t road_width = oroad.road_width >> 16;
  
        // Single Road
        if (oroad.road_ctrl == ORoad::ROAD_R0 || oroad.road_ctrl == ORoad::ROAD_R1)
        {
            x += road_width;

            static int16_t NEAR = 0xD4 - 0x4A;
            static int16_t FAR  = 0x104 - 0x4A;

            // Don't break
            if (x > -NEAR && x <= NEAR)      oinputs.brake_adjust = 0;
            // Both Wheels Off
            else if (x < -FAR || x > FAR)    oinputs.brake_adjust = OInputs::BRAKE_THRESHOLD4;
            // Right Wheel Nearly Off
            else if (x > NEAR && x <= FAR)   oinputs.brake_adjust = OInputs::BRAKE_THRESHOLD3;
            // Left Wheel Nearly Off
            else if (x <= NEAR && x >= -FAR) oinputs.brake_adjust = OInputs::BRAKE_THRESHOLD3;
        }
        // Two Roads (But we only bother braking if they are joined)
        else if (oroad.road_ctrl == ORoad::ROAD_BOTH_P0 || oroad.road_ctrl == ORoad::ROAD_BOTH_P1)
        {
            // Roads are Joined
            if (road_width <= 0xFF)
            {
                static int16_t FAR = 0x104 - 0x4A;
                road_width += FAR;
                if (x < 0) x = -x;
                if (x > road_width)             
                    oinputs.brake_adjust = OInputs::BRAKE_THRESHOLD4;  // Both Wheels Nearly Off
                else if (x > road_width - 0x30) 
                    oinputs.brake_adjust = OInputs::BRAKE_THRESHOLD3;  // One Wheel Nearly Off
            }
        }
    }

    // Brake when traffic nearby
    if (otraffic.ai_traffic)
    {
        if (oinitengine.car_increment >> 16 >= 0xF0)
            oinputs.brake_adjust = OInputs::BRAKE_THRESHOLD4;
        otraffic.ai_traffic = 0;
    }

    // --------------------------------------------------------------------------------------------
    // Acceleration: Always accelerate unless we're breaking
    // --------------------------------------------------------------------------------------------
    if (!oinputs.brake_adjust)
        oinputs.acc_adjust = 0xFF;
    else
        oinputs.acc_adjust = 0;
}


// ------------------------------------------------------------------------------------------------
//                                           ORIGINAL AI CODE
// ------------------------------------------------------------------------------------------------

// Attract Mode AI Code
//
// Source: 0xA084
void OAttractAI::tick_ai()
{
    // Check upcoming road segment for straight/curve.
    // Choose route from pre defined table at road split.
    check_road();    
    
    // Set steering value based on upcoming road segment
    set_steering();  
  
    oinputs.brake_adjust = 0;

    // If speed is below a certain amount, just accelerate
    if (oinitengine.car_increment >> 16 < 0xFA)
    {
        oinputs.acc_adjust = 0xFF; // max value
        return;
    }
  
    // If AI Traffic is close, set brake on
    if (otraffic.ai_traffic)
    {
        otraffic.ai_traffic = 0;
        oinputs.brake_adjust = OInputs::BRAKE_THRESHOLD3;
    }
  
    // If either wheel of the car is off-road, set brake on
    else if (oferrari.wheel_state != OFerrari::WHEELS_ON)
    {
        oinputs.brake_adjust = OInputs::BRAKE_THRESHOLD3;
    }
  
    // Upcoming road: Straight Road
    if (oinitengine.road_curve_next == 0)
    {
        oferrari.sprite_ai_counter = 0; // Clear AI Curve Counter
    }
    // Upcoming road: Curved Road
    else
    {
        // Increment AI Curve Counter
        if (++oferrari.sprite_ai_counter == 1)
        {
            // Set road curve value based on hard coded road data. 
            // High value = Sharper Bend
            int16_t sprite_ai_curve = 0x96 - oinitengine.road_curve_next; 
            if (sprite_ai_curve >= 0)
                oferrari.sprite_ai_curve = sprite_ai_curve;
        }
        // Curve: Toggle brake. You'll notice the brake flickers on/off in OutRun attract mode
        else if (oferrari.sprite_ai_curve)
        {
             if (oferrari.sprite_ai_curve <= 0xA || oferrari.sprite_ai_curve & BIT_3)
                 oinputs.brake_adjust = OInputs::BRAKE_THRESHOLD2;

            oferrari.sprite_ai_curve--;
        }   
    }

    // Set accelerator to max value
    oinputs.acc_adjust = 0xFF;
}

// Check upcoming road segment for straight/curve
// Check upcoming road segment for road split
//
// Source: 0xA318

void OAttractAI::check_road()
{
    // --------------------------------------------------------------------------------------------
    // Process Upcoming Curve
    // --------------------------------------------------------------------------------------------

    const int16_t STEER = 0xb4;

    // Upcoming Road: Straight or No Change
    if (oinitengine.road_type_next <= OInitEngine::ROAD_STRAIGHT)
    {
        if (oinitengine.road_type_next == OInitEngine::ROAD_STRAIGHT)
        {
            oferrari.sprite_ai_x = oinitengine.road_type == OInitEngine::ROAD_RIGHT ? STEER : -STEER;
        }
        else // NO CHANGE
        {
            if (oinitengine.road_type == OInitEngine::ROAD_LEFT)
                oferrari.sprite_ai_x = STEER;
            else if (oinitengine.road_type != OInitEngine::ROAD_RIGHT)
            {
                oferrari.sprite_ai_x = 0;
                return;
            }
            else
                oferrari.sprite_ai_x = -STEER;
        }
    }
    // Upcoming Road: Curve
    else
    {
        oferrari.sprite_ai_x = oinitengine.road_type_next == OInitEngine::ROAD_LEFT ? STEER : -STEER;
    }

    // --------------------------------------------------------------------------------------------
    // Process Road Split
    // --------------------------------------------------------------------------------------------

    if (oinitengine.rd_split_state > 0 && oinitengine.rd_split_state < 4)
    {
        // Route information for stages
        // 0 = Turn Left, 1 = Turn Right
        const uint8_t ROUTE_INFO[] = { 0, 1, 1, 0, 0 };

        if (ROUTE_INFO[ostats.cur_stage])
            oferrari.sprite_ai_x = -oferrari.sprite_ai_x;
    }
}

// Set steering value based on road split, previously set curve info
//
// Source: 0xA3C2
void OAttractAI::set_steering()
{
    int16_t steering = 0;   // d0
    int16_t car_x_diff = 0; // d1
    int16_t x_change = 0;   // d2
    int16_t x = 0;          // d3
    int16_t car_x = 0;      // d4
    
    // Mid Road Split
    if (oinitengine.rd_split_state >= 4)
    {
        x_change = oroad.road_width >> 16;           // d2
        x = oinitengine.car_x_pos;                   // d3

        // Right Route Selected
        if (oinitengine.route_selected == 0)
            x += x_change;
        else
            x -= x_change;

        car_x = x;
        x_change = oferrari.sprite_ai_x - x;
    }
    // Start Road Split / Not Road Split
    else
    {
        car_x = oinitengine.car_x_pos;
        x_change = oferrari.sprite_ai_x - car_x;
    }

    // A404
    x = x_change;
    x_change = (x_change < 0) ? -x_change : x_change;
    if (x_change > 6) 
        x_change = 6;
    // A414 RHS Of Road
    if (x >= 0)
    {
        car_x_diff = car_x - oferrari.sprite_car_x_bak;
        if (car_x_diff || x_change)
        {
            if (car_x_diff < 1) steering = -1;
            else if (car_x_diff > 1) steering = 1;
            else
            {
                // set_steering
                oferrari.sprite_car_x_bak = car_x;
                oinputs.steering_adjust   = oferrari.sprite_ai_steer;
                return;
            }
        }
        else
        {
            // set_steering
            oferrari.sprite_car_x_bak = car_x;
            oinputs.steering_adjust   = oferrari.sprite_ai_steer;
            return;
        }

    }
    // A43A - LHS Of Road
    else
    {
        car_x_diff = oferrari.sprite_car_x_bak - car_x;
        if (car_x_diff || x_change)
        {
            if (car_x_diff < 1) steering = 1;
            else if (car_x_diff > 1) steering = -1;
            else
            {
                // set_steering
                oferrari.sprite_car_x_bak = car_x;
                oinputs.steering_adjust   = oferrari.sprite_ai_steer;
                return;
            }
        }
        else
        {
            // set_steering
            oferrari.sprite_car_x_bak = car_x;
            oinputs.steering_adjust   = oferrari.sprite_ai_steer;
            return;
        }
    }

    // A462
    x_change++;

    steering = (steering * x_change) + oferrari.sprite_ai_steer;
    
    if (steering > 0x7F)
        steering = 0x7F;
    else if (steering < -0x7F)
        steering = -0x7F;

    oferrari.sprite_ai_steer  = steering;
    oinputs.steering_adjust   = steering;
    oferrari.sprite_car_x_bak = car_x;
}


// ------------------------------------------------------------------------------------------------
//                                        END OF GAME AI CODE
// ------------------------------------------------------------------------------------------------

// Bonus Mode: Set x steering adjustment
// Check upcoming road segment for straight/curve
//
// Source: 0xA498
void OAttractAI::check_road_bonus()
{
    // Upcoming Road: Straight or No Change
    if (oinitengine.road_type_next <= OInitEngine::ROAD_STRAIGHT)
    {
        if (oinitengine.road_type_next == OInitEngine::ROAD_STRAIGHT)
        {
            oferrari.sprite_ai_x = oinitengine.road_type == OInitEngine::ROAD_RIGHT ? -0xB4 : 0xB4; // different from check_road() 
        }
        else // NO CHANGE
        {
            if (oinitengine.road_type == OInitEngine::ROAD_LEFT)
                oferrari.sprite_ai_x = 0xB4;
            else if (oinitengine.road_type != OInitEngine::ROAD_RIGHT)
                oferrari.sprite_ai_x = 0;
            else
                oferrari.sprite_ai_x = -0xB4;
        }
    }
    // Upcoming Road: Curve
    else
    {
        oferrari.sprite_ai_x = oinitengine.road_type_next == OInitEngine::ROAD_LEFT ? 0xB4 : -0xB4;
    }
}

// Bonus Mode: Set steering value configured in check_road_bonus()
//
// Source: 0xA510
void OAttractAI::set_steering_bonus()
{
    int16_t steering = oinitengine.car_x_pos;      // d4

    // Road Split During Bonus Mode
    if (oinitengine.rd_split_state >= 0x14)
    {
        int16_t road_width = oroad.road_width >> 16;   // d2

        // Right Route Selected
        if (oinitengine.route_selected == 0)
            steering += road_width;
        else
            steering -= road_width;
    }

    // check_steering:
    // Check adjusted steering value is between bounds and set
    steering = oferrari.sprite_ai_x - steering;

    if (steering)
    {
        steering = -steering;

        if (steering > 0x7F)
            steering = 0x7F;
        else if (steering < -0x7F)
            steering = -0x7F;

        oinputs.steering_adjust = steering;
    }
}