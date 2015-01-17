#include "game_stuff.h"
#include "protocol.h"

#include <math.h>

int unit_calculate_cost(char type, unsigned int count)
{
    switch (type) {
    case UNIT_WORKER:
        return count * 150;
    case UNIT_LIGHT_INFANTRY:
        return count * 100;
    case UNIT_HORSEMEN:
        return count * 550;
    case UNIT_HEAVY_INFANTRY:
        return count * 250;
    }

    return 0;
}

unsigned int unit_train_time(char type)
{
    switch (type) {
    case UNIT_WORKER:
        return 2;
    case UNIT_LIGHT_INFANTRY:
        return 2;
    case UNIT_HEAVY_INFANTRY:
        return 3;
    case UNIT_HORSEMEN:
        return 5;
    }

    return 0;
}

unsigned int attack_time()
{
    return 5;
}

double attack_power(war_army_t *army)
{
    double out = 0;

    out += 1.5d * army->heavy;
    out += 1.d * army->light;
    out += 3.5d * army->horsemen;

    return out;
}

double attack_defense(war_army_t *army)
{
    double out = 0;

    out += 3.d * army->heavy;
    out += 1.2d * army->light;
    out += 1.2d * army->horsemen;

    return out;
}

war_army_t attack_getloss(double ratio, war_army_t *army)
{
    war_army_t losses;

    if (ratio >= 1.d) {
        losses.workers = 0;
        losses.heavy = army->heavy;
        losses.horsemen = army->horsemen;
        losses.light = army->light;
    } else {
        losses.workers = 0; // ?
        losses.heavy = (unsigned int) floor(ratio * army->heavy);
        losses.horsemen = (unsigned int) floor(ratio * army->horsemen);
        losses.light = (unsigned int) floor(ratio * army->light);
    }

    return losses;
}

int calculate_resource_gain(unsigned int workers)
{
    return 50 + workers * 5;
}
