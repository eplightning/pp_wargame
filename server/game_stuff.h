#ifndef GAME_STUFF_H
#define GAME_STUFF_H

#include "protocol.h"

int unit_calculate_cost(char type, unsigned int count);
unsigned int unit_train_time(char type);
int calculate_resource_gain(unsigned int workers);

unsigned int attack_time();
double attack_power(war_army_t *army);
double attack_defense(war_army_t *army);
war_army_t attack_getloss(double ratio, war_army_t *army);

#endif // GAME_STUFF_H

