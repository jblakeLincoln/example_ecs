#if 0
time clang++ main.cpp                              \
	-Wall -Wswitch-default -Wcast-align -Wpointer-arith                        \
	-Winline -Wundef -Wcast-qual -Wshadow -Wwrite-strings                      \
	-Wunreachable-code -fno-common -Wunused-function                           \
	-Wuninitialized -Wtype-limits -Wsign-compare -Wmissing-braces              \
	-Wparentheses \
	--std=c++11 -pedantic -o a.out && ./a.out
exit
#endif

#include <stdio.h>
#include "ecs.h"

struct Health {
	int data;

	Health(int h)
		: data(h)
	{}
};

struct PoisonDamage {
	int dmg_rate = 5;
};

template<>
struct System<Health> : SystemBase<Health, 0> {
	void Manage(uint64_t id, Health &h) {
		/* When health drops to or below zero, the entity can be removed. */
		if(h.data <= 0)
			mgr->GetByID(id)->Destroy();
	}
};

template<>
struct System<PoisonDamage> : SystemBase<PoisonDamage> {
	void Manage(uint64_t id, PoisonDamage &p) {
		Entity *e = mgr->GetByID(id);

		if(e->Has<Health>())
			e->Get<Health>()->data -= p.dmg_rate;
	}
};

int main() {
	EntityManager mgr;
	Entity *player = mgr.CreateEntity();
	uint64_t player_id = player->GetID();

	player->Add<Health>(15);
	player->Add<PoisonDamage>();

	/* Pretend game loop. */
	for(size_t i = 0; i < 5; ++i) {
		/* Can't assume the player is still in existence since systems can
		 * affect its lifetime.
		 */
		player = mgr.GetByID(player_id);

		if(player != nullptr)
			printf("Player health: %d\n", player->Get<Health>()->data);
		else
			printf("Player is dead\n");

		/* Processes systems defined above, usually called once per frame. */
		mgr.Manage();
	}

	return 0;
}
