#if 0
time clang++ main.cpp                                                          \
	-Wall -Wswitch-default -Wcast-align -Wpointer-arith                        \
	-Winline -Wundef -Wcast-qual -Wshadow -Wwrite-strings                      \
	-Wunreachable-code -fno-common -Wunused-function                           \
	-Wuninitialized -Wtype-limits -Wsign-compare -Wmissing-braces              \
	-Wparentheses -fno-rtti                                                    \
	--std=c++11 -pedantic -o a.out && ./a.out
exit
#endif

#include <stdio.h>
#include "ecs.h"

enum ComponentID {
	HEALTH,
	POISON_DAMAGE
};

struct Health {
	static constexpr uint16_t component_id = ComponentID::HEALTH;
	int data;

	Health(int h)
		: data(h)
	{}
};

struct PoisonDamage {
	static constexpr uint16_t component_id = ComponentID::POISON_DAMAGE;
	int dmg_rate = 5;
};

template<>
struct System<Health> : SystemBase<Health, 0> {
	void Manage(Entity &e, Health &h) {
		/* When health drops to or below zero, the entity can be removed. */
		if(h.data <= 0)
			e.Destroy();
	}
};

template<>
struct System<PoisonDamage> : SystemBase<PoisonDamage> {
	void Manage(Entity &e, PoisonDamage &p) {
		if(e.Has<Health>())
			e.Get<Health>()->data -= p.dmg_rate;
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
		/*
		 * Can't assume the player is still in existence since systems can
		 * affect its lifetime.
		 */
		player = mgr.GetByID(player_id);

		if(player != nullptr)
			printf("Player health: %d\n", player->Get<Health>()->data);
		else
			printf("Player is dead\n");

		/* Processes systems defined above, usually called once per frame. */
		mgr.Manage();

		/*
		 * Every update, we can retrieve a copy of a system, modify it, and
		 * post it. This is intended to be able to swap values in and out
		 * for interpolating during rendering.
		 */
		System<Health> *h = mgr.GetSystemCopy<Health>();
		for(size_t j = 0; j < h->components.size(); ++j)
			h->components[j].data += 1;
		mgr.ReplaceSystem<Health>(h);
	}

	return 0;
}
