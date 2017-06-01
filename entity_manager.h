#ifndef ENTITY_MANAGER_H
#define ENTITY_MANAGER_H

#include "system.h"

#include <bitset>
#include <typeindex>
#include <unordered_map>
#include <vector>

/**
 * The EntityManager is in charge of holding all entities and associated
 * systems, which store the components attached to entities.
 *
 * The "entities" and "systems" are internal interfaces for managing the
 * lifetimes of and interactions with entities and systems, some of which are
 * made public through EntityManager functions.
 */
struct Entity;
struct EntityManager {
friend Entity;
protected:
	/**
	 * The EntityManager holds an instance of a system for each type that gets
	 * attached to an entity at any point during the runtime of the application.
	 * Systems are created when first required (e.g. an entity adding a
	 * component of a new type for the first time).
	 */
	struct Systems {
	private:
		/**
		 * Dynamically allocated IDs to components as systems are created.
		 * Store systems as generic ISystem pointers, and allow looking up the
		 * component ID through the component type.
		 *
		 * We keep a separate vector that is sorted by priority, pointing to
		 * systems deemed "owned" by the systems array - though this doesn't
		 * actually come into play at any point.
		 */
		ISystem* systems[MAX_COMPONENTS];
		std::vector<ISystem*> systems_prioritised;

	public:
		Systems();
		~Systems();

		/**
		 * Called on EntityManager Manage to call each System update.
		 */
		void Manage();

		/**
		 * Store an ISystem pointer for a specific component type.
		 */
		void Add(EntityManager *mgr, uint16_t cid, ISystem*);

		/**
		 * Check if a system exists for a specific component type.
		 */
		bool Has(uint16_t cid);

		/**
		 * Retrieve system by component ID
		 */
		ISystem* Get(uint16_t cid);

		/**
		 * Remove a system and all associated components.
		 */
		void Delete(uint16_t cid);
	} systems;

	struct Entities {
	private:
		/**
		 * Allocate entity IDs as they are requested. Heap allocate entities
		 * so that pointers to them can remain valid, even though it's safest
		 * to look them up by ID to ensure they exist.
		 */
		uint64_t entity_ids = 0;
		std::unordered_map<uint64_t, Entity*> entities;

	public:
		~Entities();

		/**
		 * Request a new Entity from the manager.
		 */
		Entity* Create(EntityManager*);

		/**
		 * Retrieve an entity by its ID.
		 */
		Entity* Get(uint64_t);

		/**
		 * Destroy a provided entity and all attached components.
		 */
		void Destroy(Systems&, Entity *e);
	} entities;

	/**
	 * Retrieve a system for a specified type. If it doesn't exist, it is
	 * created.
	 */
	template<typename T>
	System<T>* GetSystem();

public:
	EntityManager() {}
	~EntityManager() {}

	/**
	 * Create and return a single entity.
	 */
	Entity *CreateEntity() {
		return entities.Create(this);
	}

	/**
	 * Retrieve an entity by its ID.
	 */
	Entity *GetByID(uint64_t id) {
		return entities.Get(id);
	}

	/**
	 * Initialise a list of entity pointers. Useful with curly brace syntax.
	 */
	void CreateEntities(std::vector<Entity**> e) {
		for(size_t i = 0; i < e.size(); ++i)
			*(e[i]) = entities.Create(this);
	}

	/**
	 * Construct a new system. Used when the default constructor for a system
	 * is unavailable for the system to be created automatically.
	 */
	template<typename T, typename ... Args>
	void AddSystem(const Args &...args);

	/**
	 * Create a clone of a system, also copying all components and pointers
	 * to entities.
	 */
	template<typename T>
	System<T> *GetSystemCopy();

	/**
	 * Replaces an existing system with a provided system. Takes ownership
	 * of the provided pointer.
	 *
	 * All entities will continue to make the same assumptions about the new
	 * system as they did about the old one. The replacement system should
	 * only modify the component data - not their storage, or entity pointers.
	 */
	template<typename T>
	void ReplaceSystem(System<T> *nsys);

	/**
	 * Calls the Update function for all systems.
	 */
	void Manage() {
		systems.Manage();
	}
};

/**
 * An Entity is a object that owns components - sets of data that are operated.
 * The entity is not a specific thing, but is a general container which is
 * composed of different properties.
 */
struct Entity {
friend EntityManager;
private:
	/**
	 * Destruction happens through the Destroy function, handled by the
	 * manager.
	 */
	~Entity() {}

	/**
	 * Stores a pointer to the EntityManager to which it belongs, the ID by
	 * which the manager and systems refer to it, and a bitset of attached
	 * components for speedy lookup of what the entity owns.
	 */
	EntityManager *mgr;
	uint64_t id;
	std::bitset<MAX_COMPONENTS> components;

protected:
	/**
	 * Construction is performed by the EntityManager, when it passes
	 * through a pointer to itself.
	 */
	Entity(EntityManager *mgr_in)
		: mgr(mgr_in)
	{}

	/**
	 * Check the entity for having a provided list of components, by
	 * iterating over types in a tuple, provided by the public Has<>().
	 */
	template<std::size_t I = 0, typename... Type>
	inline typename std::enable_if<I < sizeof...(Type), bool>::type
	HasComponent(const std::tuple<Type...> &t);

	/**
	 * Go to the next type in the HasComponent tuple.
	 */
	template<std::size_t I = 0, typename... Type>
	inline typename std::enable_if<I == sizeof...(Type), bool>::type
	HasComponent(const std::tuple<Type...> &t);

public:
	uint64_t GetID() {
		return id;
	}

	/**
	 * Construct a new component to attach to the entity, constructed in
	 * place in the relevant system with the constructor arguments
	 * provided.
	 *
	 * Returns the newly constructed component.
	 */
	template<typename T, typename... Args>
	T* Add(const Args &...args);

	/**
	 * Retrieve a component type for the entity. Returns a component if it
	 * exists, otherwise nullptr.
	 */
	template<typename T>
	T* Get();

	/**
	 * Removes a selected type of component, or does nothing if the
	 * component type is not attached.
	 */
	template<typename T>
	void Remove();

	/**
	 * Check if the system has components of the specified types. True if
	 * the entity holds all provided types, otherwise. false.
	 */
	template<typename First, typename... Types>
	bool Has();

	/**
	 * Destroy the entity, having it removed from the manager and all
	 * attached components destroyed.
	 */
	void Destroy();
};

/*************************************
 * EntityManager functions
 *************************************/
template<typename T>
System<T>* EntityManager::GetSystem()
{
	if(systems.Has(T::component_id) == false)
		systems.Add(this, T::component_id, new System<T>());

	return static_cast<System<T>*>(systems.Get(T::component_id));
}

template<typename T, typename ... Args>
void EntityManager::AddSystem(const Args &...args)
{
	if(systems.Has(T::component_id) == true)
		return;

	systems.Add(this, T::component_id, new T(args...));
}

template<typename T>
System<T> *EntityManager::GetSystemCopy()
{
	ISystem *sys = systems.Get(T::component_id);

	if(sys == nullptr)
		return nullptr;

	return new System<T>(*static_cast<System<T>*>(sys));
}

template<typename T>
void EntityManager::ReplaceSystem(System<T> *new_sys) {
	systems.Delete(T::component_id);
	systems.Add(this, T::component_id, new_sys);
}

/**************************************
 * Entity functions
 **************************************/
template<typename T, typename... Args>
T* Entity::Add(const Args &...args) {
	uint16_t cid = T::component_id;

	if(components[cid] == true)
		return mgr->GetSystem<T>()->GetComponent(this);

	T *ret = mgr->GetSystem<T>()->AddComponent(this, args...);
	components.set(cid, true);
	return ret;
}

template<typename T>
T* Entity::Get() {
	uint16_t cid = T::component_id;
	if(components[cid] == false)
		return nullptr;

	return mgr->GetSystem<T>()->GetComponent(this);
}

template<typename T>
void Entity::Remove() {
	mgr->GetSystem<T>()->RemoveComponent(this);
	components.set(T::component_id, false);
}

template<std::size_t I, typename... Type>
inline typename std::enable_if<I < sizeof...(Type), bool>::type
Entity::HasComponent(const std::tuple<Type...> &t) {
	uint16_t cid = std::get<I>(t)->component_id;
	bool ret = components[cid];

	if(ret == true)
		ret = HasComponent<I + 1, Type...>(t);

	return ret;
}

template<std::size_t I, typename... Type>
inline typename std::enable_if<I == sizeof...(Type), bool>::type
Entity::HasComponent(const std::tuple<Type...> &t) {
	return true;
}

template<typename First, typename... Types>
bool Entity::Has() {
	uint16_t cid = First::component_id;
	bool ret = components[cid];

	if(ret == false || sizeof...(Types) == 0)
		return ret;

	return HasComponent(std::tuple<Types*...>());
}

void Entity::Destroy() {
	mgr->entities.Destroy(mgr->systems, this);
}

/**************************************
 * EntityManager Systems functions
 **************************************/

EntityManager::Systems::Systems() {
	systems_prioritised.reserve(MAX_COMPONENTS);

	for(size_t i = 0; i < MAX_COMPONENTS; ++i)
		systems[i] = nullptr;
}

EntityManager::Systems::~Systems() {
	for(size_t i = 0; i < MAX_COMPONENTS; ++i) {
		if(systems[i] == nullptr)
			continue;

		delete systems[i];
		systems[i] = nullptr;
	}
}

void EntityManager::Systems::Add
(EntityManager *mgr, uint16_t cid, ISystem *sys)
{
	if(systems[cid] != nullptr)
		return;

	sys->mgr = mgr;
	systems[cid] = sys;

	size_t i;
	for(i = 0; i < systems_prioritised.size(); ++i) {
		if(systems_prioritised[i]->GetPriority() < sys->GetPriority())
			break;
	}

	systems_prioritised.insert(systems_prioritised.begin() + i, sys);
}

bool EntityManager::Systems::Has(uint16_t cid) {
	ISystem *sys = systems[cid];
	return sys != nullptr;
}

ISystem* EntityManager::Systems::Get(uint16_t cid) {
	return systems[cid];
}

void EntityManager::Systems::Delete(uint16_t cid) {
	if(systems[cid] == nullptr)
		return;

	for(size_t i = 0; i < systems_prioritised.size(); ++i) {
		if(systems_prioritised[i] == systems[cid])
			systems_prioritised.erase(systems_prioritised.begin() + i);
	}

	delete systems[cid];
	systems[cid] = nullptr;
}

void EntityManager::Systems::Manage() {
	for(size_t i = 0; i < systems_prioritised.size(); ++i)
		systems_prioritised[i]->Manage();
}

/**************************************
 * EntityManager Entities functions
 **************************************/

EntityManager::Entities::~Entities() {
	for(auto it = entities.begin(); it != entities.end(); ++it)
		delete it->second;
}

Entity* EntityManager::Entities::Create(EntityManager *mgr) {
	Entity *e = new Entity(mgr);
	entities[++entity_ids] = e;
	e->id = entity_ids;
	return e;
}

Entity* EntityManager::Entities::Get(uint64_t id) {
	if(entities.count(id) == 0)
		return nullptr;
	return entities[id];
}

void EntityManager::Entities::Destroy(Systems &mgr_systems, Entity *e) {
	for(size_t i = 0; i < e->components.size(); ++i) {
		if(e->components[i] == false)
			continue;
		mgr_systems.Get(i)->RemoveComponent(e);
	}

	entities.erase(e->id);
	delete e;
}

#endif
