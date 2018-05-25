/***************************************************************************
    Animation Sequences.
    
    Used in three areas of the game:
    - The sequence at the start with the Ferrari driving in from the side
    - Flag Waving Man
    - 5 x End Sequences
    
    See "oanimsprite.hpp" for the specific format used by animated sprites.
    It is essentially a deviation from the normal sprites in the game.
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include "engine/obonus.hpp"
#include "engine/oferrari.hpp"
#include "engine/oinputs.hpp"
#include "engine/oanimseq.hpp"

// ----------------------------------------------------------------------------
// Animation Data Format.
// Animation blocks are stored in groups of 8 bytes, and formatted as follows:
//
// +00 [Byte] Sprite Colour Palette
// +01 [Byte] Bit 7: Make X Position Negative
//            Bits 4-6: Sprite To Sprite Priority
//            Bits 0-3: Top Bits Of Sprite Data Address
// +02 [Word] Sprite Data Address
// +04 [Byte] Sprite X Position
// +05 [Byte] Sprite Y Position
// +06 [Byte] Sprite To Road Priority
// +07 [Byte] Bit 7: Set To Load Next Block Of Sprite Animation Data To 0x1E
//            Bit 6: Set For H-Flip
//            Bit 4:
//            Bits 0-3: Animation Frame Delay 
//                     (Before Incrementing To Next Block Of 8 Bytes)
// ----------------------------------------------------------------------------

OAnimSeq oanimseq;

OAnimSeq::OAnimSeq(void)
{
}


OAnimSeq::~OAnimSeq(void)
{
}

void OAnimSeq::init(oentry* jump_table)
{
    // --------------------------------------------------------------------------------------------
    // Flag Animation Setup
    // --------------------------------------------------------------------------------------------
    oentry* sprite_flag = &jump_table[OSprites::SPRITE_FLAG];
    anim_flag.sprite = sprite_flag;
    anim_flag.anim_state = 0;

    // Jump table initalisations
    sprite_flag->shadow = 7;
    sprite_flag->draw_props = oentry::BOTTOM;

    // Routine initalisations
    sprite_flag->control |= OSprites::ENABLE;
    sprite_flag->z = 400 << 16;

    // --------------------------------------------------------------------------------------------
    // Ferrari & Passenger Animation Setup For Intro
    // --------------------------------------------------------------------------------------------
    oentry* sprite_ferrari = &jump_table[OSprites::SPRITE_FERRARI];
    anim_ferrari.init(sprite_ferrari);
    anim_ferrari.anim_addr_curr = outrun.adr.anim_ferrari_curr;
    anim_ferrari.anim_addr_next = outrun.adr.anim_ferrari_next;
    sprite_ferrari->control |= OSprites::ENABLE;
    sprite_ferrari->draw_props = oentry::BOTTOM;

    oentry* sprite_pass1 = &jump_table[OSprites::SPRITE_PASS1];
    anim_pass1.init(sprite_pass1);
    anim_pass1.anim_addr_curr = outrun.adr.anim_pass1_curr;
    anim_pass1.anim_addr_next = outrun.adr.anim_pass1_next;
    sprite_pass1->draw_props = oentry::BOTTOM;

    oentry* sprite_pass2 = &jump_table[OSprites::SPRITE_PASS2];
    anim_pass2.init(sprite_pass2);
    anim_pass2.anim_addr_curr = outrun.adr.anim_pass2_curr;
    anim_pass2.anim_addr_next = outrun.adr.anim_pass2_next;
    sprite_pass2->draw_props = oentry::BOTTOM;

    // --------------------------------------------------------------------------------------------
    // End Sequence Animation
    // --------------------------------------------------------------------------------------------
    end_seq_state = 0; // init
    seq_pos = 0;
    ferrari_stopped = false;

    anim_obj1.init(&jump_table[OSprites::SPRITE_CRASH]);
    anim_obj2.init(&jump_table[OSprites::SPRITE_CRASH_SHADOW]);
    anim_obj3.init(&jump_table[OSprites::SPRITE_SHADOW]);
    anim_obj4.init(&jump_table[OSprites::SPRITE_CRASH_PASS1]);
    anim_obj5.init(&jump_table[OSprites::SPRITE_CRASH_PASS1_S]);
    anim_obj6.init(&jump_table[OSprites::SPRITE_CRASH_PASS2]);
    anim_obj7.init(&jump_table[OSprites::SPRITE_CRASH_PASS2_S]);
    anim_obj8.init(&jump_table[OSprites::SPRITE_FLAG]);
}

// ------------------------------------------------------------------------------------------------
// START ANIMATION SEQUENCES (FLAG, FERRARI DRIVING IN)
// ------------------------------------------------------------------------------------------------

void OAnimSeq::flag_seq()
{
    if (!(anim_flag.sprite->control & OSprites::ENABLE))
        return;

    if (outrun.tick_frame)
    {
        if (outrun.game_state < GS_START1 || outrun.game_state > GS_GAMEOVER)
        {
            anim_flag.sprite->control &= ~OSprites::ENABLE;
            return;
        }

        // Init Flag Sequence
        if (outrun.game_state < GS_INGAME && anim_flag.anim_state != outrun.game_state)
        {
            anim_flag.anim_state = outrun.game_state;

            // Index of animation sequences
            uint32_t index = outrun.adr.anim_seq_flag + ((outrun.game_state - 9) << 3);

            anim_flag.anim_addr_curr = roms.rom0p->read32(&index);
            anim_flag.anim_addr_next = roms.rom0p->read32(&index);

            anim_flag.frame_delay = roms.rom0p->read8(7 + anim_flag.anim_addr_curr) & 0x3F;
            anim_flag.anim_frame  = 0;
        }

        // Wave Flag 
        if (outrun.game_state <= GS_INGAME)
        {
            uint32_t index = anim_flag.anim_addr_curr + (anim_flag.anim_frame << 3);

            anim_flag.sprite->addr    = roms.rom0p->read32(index) & 0xFFFFF;
            anim_flag.sprite->pal_src = roms.rom0p->read8(index);

	        uint32_t addr = SPRITE_ZOOM_LOOKUP + (((anim_flag.sprite->z >> 16) << 2) | osprites.sprite_scroll_speed);
	        uint32_t value = roms.rom0p->read32(addr);
	        anim_flag.sprite->z += value;
            uint16_t z16 = anim_flag.sprite->z >> 16;
	    
            if (z16 >= 0x200)
	        {
                anim_flag.sprite->control &= ~OSprites::ENABLE;
		        return;
	        }
	        anim_flag.sprite->priority = z16;
	        anim_flag.sprite->zoom     = z16 >> 2;

            // Set X Position
            int16_t sprite_x = (int8_t) roms.rom0p->read8(4 + index);
            sprite_x -= oroad.road0_h[z16];
            int32_t final_x = (sprite_x * z16) >> 9;

            if (roms.rom0p->read8(1 + index) & BIT_7)
                final_x = -final_x;

            anim_flag.sprite->x = final_x;

            // Set Y Position
            int16_t sprite_y      = (int8_t) roms.rom0p->read8(5 + index);
            int16_t final_y       = (sprite_y * z16) >> 9;
            anim_flag.sprite->y   = oroad.get_road_y(z16) - final_y;

            // Set H-Flip
            if (roms.rom0p->read8(7 + index) & BIT_6)
                anim_flag.sprite->control |= OSprites::HFLIP;
            else
                anim_flag.sprite->control &= ~OSprites::HFLIP;

            // Ready for next frame
            if (--anim_flag.frame_delay == 0)
            {
                // Load Next Block Of Animation Data
                if (roms.rom0p->read8(7 + index) & BIT_7)
                {
                    anim_flag.anim_addr_curr = anim_flag.anim_addr_next;
                    anim_flag.frame_delay    = roms.rom0p->read8(7 + anim_flag.anim_addr_curr) & 0x3F;
                    anim_flag.anim_frame     = 0;
                }
                // Last Block
                else
                {
                    anim_flag.frame_delay = roms.rom0p->read8(0x0F + index) & 0x3F;
                    anim_flag.anim_frame++;
                }
            }
        }
    }

    // Order sprites
    osprites.map_palette(anim_flag.sprite);
    osprites.do_spr_order_shadows(anim_flag.sprite);
}

// Setup Ferrari Animation Sequence
// 
// Source: 0x6036
void OAnimSeq::ferrari_seq()
{
    if (!(anim_ferrari.sprite->control & OSprites::ENABLE))
        return;

    if (outrun.game_state == GS_MUSIC) return;

    anim_pass1.sprite->control |= OSprites::ENABLE;
    anim_pass2.sprite->control |= OSprites::ENABLE;

    if (outrun.game_state <= GS_LOGO)
    {
        oferrari.init_ingame();
        return;
    }

    anim_ferrari.frame_delay = roms.rom0p->read8(7 + anim_ferrari.anim_addr_curr) & 0x3F;
    anim_pass1.frame_delay   = roms.rom0p->read8(7 + anim_pass1.anim_addr_curr) & 0x3F;
    anim_pass2.frame_delay   = roms.rom0p->read8(7 + anim_pass2.anim_addr_curr) & 0x3F;

    oferrari.car_state = OFerrari::CAR_NORMAL;
    oferrari.state     = OFerrari::FERRARI_SEQ2;

    anim_seq_intro(&anim_ferrari);
}

// Process Animations for Ferrari and Passengers on intro
//
// Source: 60AE
void OAnimSeq::anim_seq_intro(oanimsprite* anim)
{
    if (outrun.game_state <= GS_LOGO)
    {
        oferrari.init_ingame();
        return;
    }

    if (outrun.tick_frame)
    {
        if (anim->anim_frame >= 1)
            oferrari.car_state = OFerrari::CAR_ANIM_SEQ;

        uint32_t index              = anim->anim_addr_curr + (anim->anim_frame << 3);

        anim->sprite->addr          = roms.rom0p->read32(index) & 0xFFFFF;
        anim->sprite->pal_src       = roms.rom0p->read8(index);
        anim->sprite->zoom          = 0x7F;
        anim->sprite->road_priority = 0x1FE;
        anim->sprite->priority      = 0x1FE - ((roms.rom0p->read16(index) & 0x70) >> 4);

        // Set X
        int16_t sprite_x = (int8_t) roms.rom0p->read8(4 + index);
        int32_t final_x = (sprite_x * anim->sprite->priority) >> 9;
        if (roms.rom0p->read8(1 + index) & BIT_7)
            final_x = -final_x;
        anim->sprite->x = final_x;

        // Set Y
        anim->sprite->y = 221 - ((int8_t) roms.rom0p->read8(5 + index));

        // Set H-Flip
        if (roms.rom0p->read8(7 + index) & BIT_6)
            anim->sprite->control |= OSprites::HFLIP;
        else
            anim->sprite->control &= ~OSprites::HFLIP;

        // Ready for next frame
        if (--anim->frame_delay == 0)
        {
            // Load Next Block Of Animation Data
            if (roms.rom0p->read8(7 + index) & BIT_7)
            {
                // Yeah the usual OutRun code hacks to do really odd stuff!
                // In this case, to exit the routine and setup the Ferrari on the last entry for passenger 2
                if (anim == &anim_pass2)
                {
                    if (oroad.get_view_mode() != ORoad::VIEW_INCAR)
                    {
                        osprites.map_palette(anim->sprite);
                        osprites.do_spr_order_shadows(anim->sprite);
                    }
                    oferrari.init_ingame();
                    return;
                }

                anim->anim_addr_curr = anim->anim_addr_next;
                anim->frame_delay    = roms.rom0p->read8(7 + anim->anim_addr_curr) & 0x3F;
                anim->anim_frame     = 0;
            }
            // Last Block
            else
            {
                anim->frame_delay = roms.rom0p->read8(0x0F + index) & 0x3F;
                anim->anim_frame++;
            }
        }
    }

    // Order sprites
    if (oroad.get_view_mode() != ORoad::VIEW_INCAR)
    {
        osprites.map_palette(anim->sprite);
        osprites.do_spr_order_shadows(anim->sprite);
    }
}

// ------------------------------------------------------------------------------------------------
// END ANIMATION SEQUENCES
// ------------------------------------------------------------------------------------------------

// Initialize end sequence animations on game complete
// Source: 0x9978
void OAnimSeq::init_end_seq()
{
    // Process animation sprites instead of normal routine
    oferrari.state = OFerrari::FERRARI_END_SEQ;

    // Setup Ferrari Sprite
    anim_ferrari.sprite->control |= OSprites::ENABLE; 
    anim_ferrari.sprite->id = 0;
    anim_ferrari.sprite->draw_props = oentry::BOTTOM;
    anim_ferrari.anim_frame = 0;
    anim_ferrari.frame_delay = 0;

    seq_pos = 0;

    // Disable Passenger Sprites. These are replaced with new versions by the animation sequence.
    oferrari.spr_pass1->control &= ~OSprites::ENABLE;
    oferrari.spr_pass2->control &= ~OSprites::ENABLE;

    obonus.bonus_control += 4;
}

void OAnimSeq::tick_end_seq()
{
    switch (end_seq_state)
    {
        case 0: // init
            if (outrun.tick_frame) init_end_sprites();
            else return;
            
        case 1: // tick & blit
            anim_seq_outro_ferrari();                   // Ferrari Sprite
            anim_seq_outro(&anim_obj1);                 // Car Door Opening Animation
            anim_seq_outro(&anim_obj2);                 // Interior of Ferrari
            anim_seq_shadow(&anim_ferrari, &anim_obj3); // Car Shadow
            anim_seq_outro(&anim_pass1);                // Man Sprite
            anim_seq_shadow(&anim_pass1, &anim_obj4);   // Man Shadow
            anim_seq_outro(&anim_pass2);                // Female Sprite
            anim_seq_shadow(&anim_pass2, &anim_obj5);   // Female Shadow
            anim_seq_outro(&anim_obj6);                 // Man Presenting Trophy
            if (end_seq == 4)
                anim_seq_outro(&anim_obj7);             // Varies
            else
                anim_seq_shadow(&anim_obj6, &anim_obj7);
            anim_seq_outro(&anim_obj8);                 // Effects
            break;
    }
}

// Initalize Sprites For End Sequence.
// Source: 0x588A
void OAnimSeq::init_end_sprites()
{
    // Ferrari Object [0x5B12 entry point]
    uint32_t addr = outrun.adr.anim_endseq_obj1 + (end_seq << 3);
    anim_ferrari.anim_addr_curr = roms.rom0p->read32(&addr);
    anim_ferrari.anim_addr_next = roms.rom0p->read32(&addr);
    ferrari_stopped = false;
    
    // 0x58A4: Car Door Opening Animation [seq_sprite_entry]
    anim_obj1.sprite->control |= OSprites::ENABLE;
    anim_obj1.sprite->id = 1;
    anim_obj1.sprite->shadow = 3;
    anim_obj1.sprite->draw_props = oentry::BOTTOM;
    anim_obj1.anim_frame = 0;
    anim_obj1.frame_delay = 0;
    anim_obj1.anim_props = 0;
    addr = outrun.adr.anim_endseq_obj2 + (end_seq << 3);
    anim_obj1.anim_addr_curr = roms.rom0p->read32(&addr);
    anim_obj1.anim_addr_next = roms.rom0p->read32(&addr);
    
    // 0x58EC: Interior of Ferrari (Note this wobbles a little when passengers exit) [seq_sprite_entry]
    anim_obj2.sprite->control |= OSprites::ENABLE;
    anim_obj2.sprite->id = 2;
    anim_obj2.sprite->draw_props = oentry::BOTTOM;
    anim_obj2.anim_frame = 0;
    anim_obj2.frame_delay = 0;
    anim_obj2.anim_props = 0;
    addr = outrun.adr.anim_endseq_obj3 + (end_seq << 3);
    anim_obj2.anim_addr_curr = roms.rom0p->read32(&addr);
    anim_obj2.anim_addr_next = roms.rom0p->read32(&addr);

    // 0x592A: Car Shadow [SeqSpriteShadow]
    anim_obj3.sprite->control |= OSprites::ENABLE;
    anim_obj3.sprite->id = 3;
    anim_obj3.sprite->draw_props = oentry::BOTTOM;
    anim_obj3.anim_frame = 0;
    anim_obj3.frame_delay = 0;
    anim_obj3.anim_props = 0;
    anim_obj3.sprite->addr = outrun.adr.shadow_data;

    // 0x5960: Man Sprite [seq_sprite_entry]
    anim_pass1.sprite->control |= OSprites::ENABLE;
    anim_pass1.sprite->id = 4;
    anim_pass1.sprite->draw_props = oentry::BOTTOM;
    anim_pass1.anim_frame = 0;
    anim_pass1.frame_delay = 0;
    anim_pass1.anim_props = 0;
    addr = outrun.adr.anim_endseq_obj4 + (end_seq << 3);
    anim_pass1.anim_addr_curr = roms.rom0p->read32(&addr);
    anim_pass1.anim_addr_next = roms.rom0p->read32(&addr);

    // 0x5998: Man Shadow [SeqSpriteShadow]
    anim_obj4.sprite->control = OSprites::ENABLE;
    anim_obj4.sprite->id = 5;
    anim_obj4.sprite->shadow = 7;
    anim_obj4.sprite->draw_props = oentry::BOTTOM;
    anim_obj4.anim_frame = 0;
    anim_obj4.frame_delay = 0;
    anim_obj4.anim_props = 0;
    anim_obj4.sprite->addr = outrun.adr.shadow_data;

    // 0x59BE: Female Sprite [seq_sprite_entry]
    anim_pass2.sprite->control |= OSprites::ENABLE;
    anim_pass2.sprite->id = 6;
    anim_pass2.sprite->draw_props = oentry::BOTTOM;
    anim_pass2.anim_frame = 0;
    anim_pass2.frame_delay = 0;
    anim_pass2.anim_props = 0;
    addr = outrun.adr.anim_endseq_obj5 + (end_seq << 3);
    anim_pass2.anim_addr_curr = roms.rom0p->read32(&addr);
    anim_pass2.anim_addr_next = roms.rom0p->read32(&addr);

    // 0x59F6: Female Shadow [SeqSpriteShadow]
    anim_obj5.sprite->control = OSprites::ENABLE;
    anim_obj5.sprite->id = 7;
    anim_obj5.sprite->shadow = 7;
    anim_obj5.sprite->draw_props = oentry::BOTTOM;
    anim_obj5.anim_frame = 0;
    anim_obj5.frame_delay = 0;
    anim_obj5.anim_props = 0;
    anim_obj5.sprite->addr = outrun.adr.shadow_data;

    // 0x5A2C: Person Presenting Trophy [seq_sprite_entry]
    anim_obj6.sprite->control |= OSprites::ENABLE;
    anim_obj6.sprite->id = 8;
    anim_obj6.sprite->draw_props = oentry::BOTTOM;
    anim_obj6.anim_frame = 0;
    anim_obj6.frame_delay = 0;
    anim_obj6.anim_props = 0;
    addr = outrun.adr.anim_endseq_obj6 + (end_seq << 3);
    anim_obj6.anim_addr_curr = roms.rom0p->read32(&addr);
    anim_obj6.anim_addr_next = roms.rom0p->read32(&addr);

    // Alternate Use Based On End Sequence
    anim_obj7.sprite->control |= OSprites::ENABLE;
    anim_obj7.sprite->id = 9;
    anim_obj7.sprite->draw_props = oentry::BOTTOM;
    anim_obj7.anim_frame = 0;
    anim_obj7.frame_delay = 0;
    anim_obj7.anim_props = 0;

    // [seq_sprite_entry]
    if (end_seq == 4)
    {
        addr = outrun.adr.anim_endseq_objB + (end_seq << 3);
        anim_obj7.anim_addr_curr = roms.rom0p->read32(&addr);
        anim_obj7.anim_addr_next = roms.rom0p->read32(&addr);
    }
    // Trophy Shadow
    else
    {
        anim_obj7.sprite->shadow = 7;
        anim_obj7.sprite->addr = outrun.adr.shadow_data;
    }

    // 0x5AD0: Enable After Effects (e.g. cloud of smoke for genie) [seq_sprite_entry]
    anim_obj8.sprite->control |= OSprites::ENABLE;
    anim_obj8.sprite->id = 10;
    anim_obj8.sprite->draw_props = oentry::BOTTOM;
    anim_obj8.anim_frame = 0;
    anim_obj8.frame_delay = 0;
    anim_obj8.anim_props = 0xFF00;
    addr = outrun.adr.anim_endseq_obj7 + (end_seq << 3);
    anim_obj8.anim_addr_curr = roms.rom0p->read32(&addr);
    anim_obj8.anim_addr_next = roms.rom0p->read32(&addr);
    
    end_seq_state = 1;
}

// Ferrari Outro Animation Sequence
// Source: 0x5B12
void OAnimSeq::anim_seq_outro_ferrari()
{
    if (/*outrun.tick_frame && */!ferrari_stopped)
    {
        // Car is moving. Turn Brake On.
        if (oinitengine.car_increment >> 16)
        {
            oferrari.auto_brake  = true;
            oinputs.brake_adjust = 0xFF;
        }
        else
        {
            osoundint.queue_sound(sound::VOICE_CONGRATS);
            ferrari_stopped = true;
        }
    }
    anim_seq_outro(&anim_ferrari);
}

// End Sequence: Setup Animated Sprites 
// Source: 0x5B42
void OAnimSeq::anim_seq_outro(oanimsprite* anim)
{
    oinputs.steering_adjust = 0;

    // Return if no animation data to process
    if (!read_anim_data(anim)) 
        return;

    if (outrun.tick_frame)
    {    
        // Process Animation Data
        uint32_t index = anim->anim_addr_curr + (anim->anim_frame << 3);

        anim->sprite->addr          = roms.rom0p->read32(index) & 0xFFFFF;
        anim->sprite->pal_src       = roms.rom0p->read8(index);
        anim->sprite->zoom          = roms.rom0p->read8(6 + index) >> 1;
        anim->sprite->road_priority = roms.rom0p->read8(6 + index) << 1;
        anim->sprite->priority      = anim->sprite->road_priority - ((roms.rom0p->read16(index) & 0x70) >> 4); // (bits 4-6)
        anim->sprite->x             = (roms.rom0p->read8(4 + index) * anim->sprite->priority) >> 9;
    
        if (roms.rom0p->read8(1 + index) & BIT_7)
            anim->sprite->x = -anim->sprite->x;

        // set_sprite_xy: (similar to flag code again)

        // Set Y Position
        int16_t sprite_y = (int8_t) roms.rom0p->read8(5 + index);
        int16_t final_y  = (sprite_y * anim->sprite->priority) >> 9;
        anim->sprite->y  = oroad.get_road_y(anim->sprite->priority) - final_y;

        // Set H-Flip
        if (roms.rom0p->read8(7 + index) & BIT_6)
            anim->sprite->control |= OSprites::HFLIP;
        else
            anim->sprite->control &= ~OSprites::HFLIP;

        // Ready for next frame
        if (--anim->frame_delay == 0)
        {
            // Load Next Block Of Animation Data
            if (roms.rom0p->read8(7 + index) & BIT_7)
            {
                anim->anim_props    |= 0xFF;
                anim->anim_addr_curr = anim->anim_addr_next;
                anim->frame_delay    = roms.rom0p->read8(7 + anim->anim_addr_curr) & 0x3F;
                anim->anim_frame     = 0;
            }
            // Last Block
            else
            {
                anim->frame_delay = roms.rom0p->read8(0x0F + index) & 0x3F;
                anim->anim_frame++;
            }
        } 
        osprites.map_palette(anim->sprite);
    }

    // Order sprites
    osprites.do_spr_order_shadows(anim->sprite);
}

// Render Sprite Shadow For End Sequence
// Use parent sprite as basis to set this up
// Source: 0x5C48
void OAnimSeq::anim_seq_shadow(oanimsprite* parent, oanimsprite* anim)
{
    // Return if no animation data to process
    if (!read_anim_data(anim)) 
        return;

    if (outrun.tick_frame)
    {  
        uint8_t zoom_shift = 3;

        // Car Shadow
        if (anim->sprite->id == 3)
        {
            zoom_shift = 1;
            if ((parent->anim_props & 0xFF) == 0 && oferrari.sprite_ai_x <= 5)
                zoom_shift++;
        }
        // 5C88 set_sprite_xy:
        anim->sprite->x    = parent->sprite->x;
        uint16_t priority  = parent->sprite->road_priority >> zoom_shift;
        anim->sprite->zoom = priority - (priority >> 2);
        anim->sprite->y    = oroad.get_road_y(parent->sprite->road_priority);
    
        // Chris - The following extra line seems necessary due to the way I set the sprites up.
        // Actually, I think it's a bug in the original game, relying on this being setup by 
        // the crash code previously. But anyway!
        anim->sprite->road_priority = parent->sprite->road_priority;
    }

    osprites.do_spr_order_shadows(anim->sprite);
}

// Read Animation Data for End Sequence
// Source: 0x5CC4
bool OAnimSeq::read_anim_data(oanimsprite* anim)
{
    uint32_t addr = outrun.adr.anim_end_table + (end_seq << 2) + (anim->sprite->id << 2) +  (anim->sprite->id << 4); // a0 + d1

    // Read start & end position in animation timeline for this object
    int16_t start_pos = roms.rom0p->read16(addr);     // d0
    int16_t end_pos =   roms.rom0p->read16(2 + addr); // d3

    int16_t pos = seq_pos; // d1
    
    // Global Sequence Position: Advance to next position
    // Not particularly clean embedding this here obviously!
    if (outrun.tick_frame && anim->anim_props & 0xFF00)
        seq_pos++;

    // Test Whether Animation Sequence Is Over & Initalize Course Map
    if (obonus.bonus_control != OBonus::BONUS_DISABLE)
    {
        const uint16_t END_SEQ_LENGTHS[] = {0x244, 0x244, 0x244, 0x190, 0x258};

        if (seq_pos == END_SEQ_LENGTHS[end_seq])
        {
            obonus.bonus_control = OBonus::BONUS_DISABLE;
            // we're missing all the code here to disable the animsprites, but probably not necessary?

            if (outrun.cannonball_mode == Outrun::MODE_ORIGINAL)
                outrun.game_state = GS_INIT_MAP;
            else
                outrun.init_best_outrunners();
        }
    }

    // --------------------------------------------------------------------------------------------
    // Process Animation Sequence
    // --------------------------------------------------------------------------------------------

    const bool DO_NOTHING = false;
    const bool PROCESS    = true;

    // check_seq_pos:
    // Sequence: Start Position
    // ret_set_frame_delay: 
    if (pos == start_pos)
    {
        // If current animation block is set, extract frame delay
        if (anim->anim_addr_curr)
            anim->frame_delay = roms.rom0p->read8(7 + anim->anim_addr_curr) & 0x3F;

        return PROCESS;
    }
    // Sequence: Invalid Position
    else if (pos < start_pos || pos > end_pos) // d1 < d0 || d1 > d3
        return DO_NOTHING;

    // Sequence: In Progress
    else if (pos < end_pos)
        return PROCESS;
    
    // Sequence: End Position
    else
    {
        // End Of Animation Data
        if (anim->sprite->id == 8) // Trophy person
        {
            anim->sprite->id = 11;
            if (end_seq >= 2)
                anim->sprite->shadow = 7;

            anim->anim_addr_curr = roms.rom0p->read32(outrun.adr.anim_endseq_obj8 + (end_seq << 3));
            anim->anim_addr_next = roms.rom0p->read32(outrun.adr.anim_endseq_obj8 + (end_seq << 3) + 4);
            anim->anim_frame = 0;
            return DO_NOTHING;
        }
        // 5e14
        else if (anim->sprite->id == 10)
        {
            anim->sprite->id = 12;
            if (end_seq >= 2)
                anim->sprite->shadow = 7;

            anim->anim_addr_curr = roms.rom0p->read32(outrun.adr.anim_endseq_objA + (end_seq << 3));
            anim->anim_addr_next = roms.rom0p->read32(outrun.adr.anim_endseq_objA + (end_seq << 3) + 4);
            anim->anim_frame = 0;
            return DO_NOTHING;
        }
    }
    return PROCESS;
}