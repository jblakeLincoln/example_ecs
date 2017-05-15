# Example ECS

This is a small example of an entity component system using C++11 templates.

There are two examples:
 * The master branch uses RTTI for dynamically assigning component IDs at
   runtime.
 * The "no_rtti" branch compiles with -fno-rtti and requires component structs
   to have a static integer component, but the iterface is otehrwise identical.

The system aims to be a flexible typesafe system with no difficult maintenance
and no sly "gotcha"s.

It is __not__ a complete working system, but a close approximation to something
that could ship. Possible changes or alongside additions include:

* Use of STL containers. The STL is used to make this a readable and accessible
  example, but make the compile time slower than it needs to be, and debug
  speed **far** too slow.
* System Manage functions probably want some other data sent (like game timing)
* Entity storage/lookup by string tag
* Messaging/event integration
* System separation (e.g. separate updates for logic and rendering)
* Systems for sets of components (multi-component systems)
* Possibly using RTTI to provide debug information on composed entities

## Components
Components are simply sets of data that can be attached to an entity. They don't
contain any game logic, but might contain getters and setters. A component
example might be:

``` cpp
struct Position {
	float x;
	float y;
};
```

Components are attached to an entity at runtime, and are entirely dynamic - the
entity requires no compile time knowledge or what may later be attached to it.
Any struct or class can be a component, though the intention is that they are
just data, and the magic happens elsewhere.

Components require **copy constructors** and **move constructors**, but nothing
else.

## Entities

Entities are generic containers to which sets of data (components) can be
attached. Entities belong to an EntityManager, which controls their lifetime and
is responsible for the behind the scenes work of allocating components.

Entities are created like so:
``` cpp
EntityManager mgr;
Entity *player = mgr.CreateEntity();
```

Entities can have components attached with in-place construction:

``` cpp
struct Position { ... };

player->Add<Position>();
```

The `Add` function takes variadic arguments for the constructor of a component,
like so:

``` cpp
struct Position {
	float x;
	float y;

	Position(float x_in, float y_in)
		: x(x_in)
		, y(y_in)
	{}
};

player->Add<Position>(64, 32);
```

Components and their data can be retrieved:

``` cpp
float x = player->Get<Position>()->x;
```

Get returns a `nullptr` if a component is not attached to an entity, so we can
check what an entity is composed of prior to retrieval:

``` cpp
bool b = player->Has<Position, Health>();
```

When an entity is no longer required, it can be destroyed. This also destroys
all instances of attached components.

``` cpp
player->Destroy();
```

When dealing with entities which may or may not exist, we can refer to them by
ID and look them up.

``` cpp
uint64_t player_id = player->GetID();
Entity *player_ref = mgr.GetByID(player_id);
/* player_ref is a nullptr if the entity is not found. */
```

## Systems

Systems control the processing of data within components. Systems have a Manage
function, which is called on an EntityManager `Manage` call, which is typically
made once per frame.

``` cpp
template<>
struct System<Health> : SystemBase<Health> {
	void Manage(Entity &e, Health &h) {
		if(h.data <= 0) {
			/* Perform some death logic. */
		}
	}
};
```

Systems can access the EntityManager to which they belong to create interations
between entities. Another system might be this:

``` cpp
template<>
struct System<PoisonDamage> : System<PoisonDamage> {
	void Manage(Entity &e, PoisonDamage &p) {
		if(e.Has<Health>())
			e.Get<Health>()->data -= p->damage_rate;
	}
};
```

## Full example

Components and Systems are __implicit__. They don't have to be registered to be
used. In development this offers simple iteration by not requiring the
maintenance of registering systems to an EntityManager, whilst also keeping
everything type safe. A full example of the above code follows:

``` cpp
#include "ecs.h"

struct Health {
	int data;
	Health(int h)
		: data(h)
	{}
};

struct PoisonDamage {
	int damage_rate = 5;
};

template<>
struct System<PoisonDamage> : SystemBase<PoisonDamage> {
	void Manage(Entity &e, PoisonDamage &p) {
		/* ID in the Manage function is guaranteed to be valid. */
		if(e.Has<Health>())
			e.Get<Health>()->data -= p->damage_rate;
	}
};

int main()
{
	EntityManager mgr;
	Entity *player = mgr.CreateEntity();

	player->Add<Health>(100);
	player->Add<PoisonDamage>();

	for(int i = 0; i < 5; ++i) /* This would typically be a game loop */
		mgr.Manage();

	/* Prints '75'. */
	printf("Player health: %d\n", player->Get<Health>()->data);

	return 0;
}
```

At some point it may be desired to add a system to Health, which can kill an
entity when its health drops to a certain point. When this happens, it's
important that Health is managed after anything can affect Health. To
accommodate this, Systems have a compile time priority that can be specified
like so:

``` cpp
struct System<Health> : SystemBase<Health, 0>
```

This sets Health to the lowest possible priority, meaning it will only be run
after systems with a priority of 1 or higher. The default priority is
UINT8_MAX, up to a cap of UINT32_MAX.

An EntityManager destroys its entities, along with all of their attached
components, on destruction. The above example cleans up all allocations.
