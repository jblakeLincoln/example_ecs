#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>
#include <vector>

/**
 * A system holds a number of components, and iterates over those components
 * on the "Manage" call, performing the implemented functionality.
 *
 * A system does not know about the entity type, but simply associates
 * components with entity IDs. The EntityManager tells a system when to
 * add or remove a component from a system, and the relevant entity ID
 * for that action.
 */

struct Entity;
struct EntityManager;

/**
 * ISystem is used for storing component-specific systems in generic
 * containers.
 */
struct ISystem {
friend Entity;
friend EntityManager;
protected:
	/**
	 * Mgr pointer is set to the owning manager by that manager.
	 */
	EntityManager *mgr = nullptr;

	virtual ~ISystem() {}

	/**
	 * Perform update logic for components owned by the system.
	 */
	virtual void Manage() = 0;

	/**
	 * Remove a component for the specified entity.
	 */
	virtual void RemoveComponent(Entity*) = 0;

	virtual uint32_t GetPriority() const = 0;
};

/**
 * SystemBase implements the component-specific logic that ISystem can't
 * handle.
 */
template<typename C, uint32_t Priority = UINT8_MAX>
struct SystemBase : ISystem {
friend Entity;
friend EntityManager;
protected:
	size_t it = 0;

public:
	/**
	 * Components are stored contiguously in memory. "components" and "ids"
	 * mirror each other, with entity IDs relating to components at the same
	 * index.
	 */
	std::vector<C> components;
	std::vector<Entity*> entities;

	/**
	 * Move a newly constructed component into the components list and store
	 * the ID of the entity to which it is linked.
	 */
	template<typename... Args>
	C *AddComponent(Entity *e, Args &&...args) {
		C *out = GetComponent(e);

		if(out != nullptr)
			return out;

		components.emplace_back(args...);
		entities.push_back(e);
		OnAdd(*e, components.back());

		return &components.back();
	}

	/**
	 * Remove a component for a specified entity.
	 */
	void RemoveComponent(Entity *e) {
		for(size_t i = 0; i < components.size(); ++i) {
			if(entities[i] != e)
				continue;

			if(i <= it)
				--it;

			OnRemove(*e, components[i]);
			components.erase(components.begin() + i);
			entities.erase(entities.begin() + i);
			break;
		}
	}

	/**
	 * Retrieve a component for a specified entity, or nullptr if it is not
	 * found.
	 */
	C *GetComponent(Entity *e) {
		size_t i = 0;
		for(i = 0; i < entities.size(); ++i) {
			if(entities[i] == e)
				break;
		}

		if(i == entities.size())
			return nullptr;

		return &components[i];
	}

	/**
	 * Default behaviour for a system is to call Manage for each stored
	 * component. Can be overridden to do something else.
	 */
	virtual void Manage() {
		for(it = 0; it < components.size(); ++it) {
			Manage(*entities[it], components[it]);
		}
	}

	/**
	 * Per-component update to optionally override.
	 */
	virtual void Manage(Entity &e, C &c) { };

	/**
	 * Per-component override called immediately after component construction.
	 */
	virtual void OnAdd(Entity &e, C &c) { };

	/**
	 * Per-component override called immediately before component destruction.
	 */
	virtual void OnRemove(Entity &e, C &c) { };

	/**
	 * Return the compile time priority of the system - higher is sooner.
	 */
	uint32_t GetPriority() const {
		return Priority;
	}
};

/**
 * Default System for components without a system specialisation. Does nothing
 * for the system update.
 */
template<typename C>
struct System : SystemBase<C> {
	virtual ~System() {}
};

#endif
