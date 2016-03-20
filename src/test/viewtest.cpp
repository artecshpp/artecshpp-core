#include <functional>
#include <iterator>
#include <iostream>

#include <artecshpp/core/entity.h>

typedef artecshpp::core::Entity Entity;
typedef artecshpp::core::Aspect Aspect;
typedef artecshpp::core::EntityManager EntityManager;

/**** EntityManager ***************************************/
/**** Iterator Factories ******************************************/

struct ForwardIteratorFactory {
	
	ForwardIteratorFactory( EntityManager& emgr, artecshpp::core::Aspect& aspect ) {
		
	}
	
	static inline std::vector<Entity>::iterator begin( std::vector<Entity>& entities ) {
		return entities.begin();
	}
	
	static inline std::vector<Entity>::iterator end( std::vector<Entity>& entities ) {
		return entities.end();
	}
	
};

struct CheckerIteratorFactory {
	
	Aspect& m_aspect;
	EntityManager& m_emgr;
	
	CheckerIteratorFactory( EntityManager& emgr, Aspect& aspect )
		: m_aspect(aspect), m_emgr(emgr) { }
	
	template <typename Container>
	struct iterator : public std::iterator<std::input_iterator_tag, Entity> {
		
		typename Container::iterator wrapped;
		typename Container::iterator end;
		Aspect& m_aspect;
		EntityManager& m_emgr;
		
		iterator( EntityManager& emgr, Aspect& aspect, Container& container )
			: m_aspect(aspect), m_emgr(emgr)
		{
			wrapped = container.begin();
			end = container.end();
			next_valid();
		}
		
		iterator( EntityManager& emgr, Aspect& aspect, typename Container::iterator it )
			: m_aspect(aspect), m_emgr(emgr)
		{
			wrapped = it;
		}
		
		void next_valid()
		{
			while( !m_aspect.fits(m_emgr.getBits(*wrapped)) && wrapped != end )
			{
				wrapped++;
			}
		}
		
		iterator<Container>& operator++()
		{
			wrapped++;
			next_valid();
			return *this;
		}
		
		const Entity operator * () const
		{
			return *wrapped;
		}
		
		Entity operator * ()
		{
			return *wrapped;
		}
		
		bool operator == (const iterator& rhs)
		{
			return wrapped == rhs.wrapped;
		}

		bool operator != (const iterator& rhs)
		{
			return wrapped != rhs.wrapped;
		}

	};
	
	inline iterator<std::vector<Entity>> begin( std::vector<Entity>& entities ) {
		return iterator<std::vector<Entity>>( m_emgr, m_aspect, entities );
	}
	
	inline iterator<std::vector<Entity>> end( std::vector<Entity>& entities ) {
		return iterator<std::vector<Entity>>( m_emgr, m_aspect, entities.end() );
	}
	

};


/**** EntityFilters ************************************************/

// pass to EntityFilters a template parameter indicating the bit checker

struct BaseFilter {
	BaseFilter(Aspect& aspect)
		: m_aspect(aspect) {}
	artecshpp::core::Aspect m_aspect;
};

struct AliveFilter : public BaseFilter {
	AliveFilter(EntityManager& eMgr, Aspect& aspect)
		: m_eMgr(eMgr), BaseFilter(aspect) {}
	EntityManager m_eMgr;
	
	std::vector<Entity>& entities() {
		return m_eMgr.alive();
	}
};

struct StorageFilter : public artecshpp::core::IEntityListener, public BaseFilter {
	StorageFilter(EntityManager& eMgr, Aspect& aspect)
		: m_eMgr(eMgr), BaseFilter(aspect) {}
	EntityManager m_eMgr;
	
	std::vector<Entity>& entities() {
		return m_entities;
	}
	
	void entityAdded(Entity* e) override {
		// if component bits ok
		m_entities.push_back(*e);
	}
	
	void entityRemoved(Entity* e) override {
		
	}

	std::vector<Entity> m_entities;
};


/**** Entity Views **************************************************/
// probably pass also EntityFilter <BitChecker>
template <class EntityFilter, class IteratorFactory>
struct View {
	View( EntityManager& emgr, Aspect& aspect )
		: m_eMgr(emgr), m_filter(emgr, aspect), m_itFactory(emgr, aspect)
	{
		m_eMgr.tryAddListener<EntityFilter>(m_filter);
	}
	
	// if is_same StorageFilter, remove observer from emgr

	EntityFilter m_filter;
	IteratorFactory m_itFactory;

	template <typename... Args>
	void each(std::function<void(Entity,Args&...)> f) {
		auto it = m_itFactory.begin( m_filter.entities() );
		while( it != m_itFactory.end( m_filter.entities() ) ) {
			f( *it, (m_eMgr.template getComponent<Args>(*it)) ... );
			++it;
		}
	}

	template <typename... Args>
	void each(Entity e, std::function<void(Entity,Args&...)> f) {
		f( e, (m_eMgr.template getComponent<Args>(e)) ... );
	}
	
	EntityManager& m_eMgr;
};

/**** Systems **************************************************/
class BaseSystem {
public:
	virtual ~BaseSystem() = 0;
	virtual void process() = 0;
	virtual void process(Entity e) = 0;
};

BaseSystem::~BaseSystem() {
	
}



template <typename Derived, typename ViewType, typename... Components>
class System : public BaseSystem {
public:

	artecshpp::core::Aspect m_aspect;
	
	System( EntityManager& emgr )
		: m_emgr(emgr), m_view(emgr, m_aspect) {
		// these components need to be there because those will be the queried components
		m_aspect.all<Components...>();
	}
	
	~System() {
		
	}
	
	void registerAspect( )
	{
		m_view.setBits( m_aspect );
	}

	void process() override {
		m_view.template each<Components...>(static_cast<Derived*>(this)->function);
	}
	
	void process(Entity e) override {
		m_view.template each<Components...>(e, static_cast<Derived*>(this)->function);
	}
	
private:
	ViewType m_view;	
	EntityManager m_emgr;
	
};


class SampleSystem : public System<
	SampleSystem, 									// system type
	View<AliveFilter, ForwardIteratorFactory>, 		// view type
	int, double, std::string> { 					// needed components
public:
	SampleSystem(EntityManager& emgr)
	: System(emgr) { }
	
	std::function<void(Entity e, int& i, double& d, std::string& str)> function =
		[](Entity e, int& i, double& d, std::string& s) {
			std::cout 	<< i << std::endl
						<< d << std::endl
						<< s << std::endl;
		};
};

class SampleSystem2 : public System<
	SampleSystem2, 									// system type
	View<AliveFilter, CheckerIteratorFactory>, 		// view type
	float> { 					// needed components
public:
	SampleSystem2(EntityManager& emgr)
	: System(emgr) { }
	
	std::function<void(Entity, float&)> function =
		[](Entity e, float& f) {
			std::cout << f << std::endl;
		};
};

class SampleSystem3 : public System<
	SampleSystem3, 									// system type
	View<StorageFilter, ForwardIteratorFactory>, 		// view type
	float> { 					// needed components
public:
	SampleSystem3(EntityManager& emgr)
	: System(emgr) { }
	
	std::function<void(Entity, float&)> function =
		[](Entity e, float& f) {
			std::cout << f << std::endl;
		};
};


/***************************************************************/
/*
world::process() {
	processAllSystems();

	for( gds : gravediggerSystems ) {
		gds.process();
	}

	template <T>
	gds.process() {
		for( e : deadentities ) {
			eMgr->deleteComponent<T>(e);
		}
	}

}
*/

int main( int argc, char** argv ) {

	EntityManager emgr;

	Entity e0 = emgr.createEntity();

	int& e1i = emgr.createComponent<int>(e0);
	double& e1d = emgr.createComponent<double>(e0);
	std::string& e1s = emgr.createComponent<std::string>(e0);
	float& e1f = emgr.createComponent<float>(e0);
	e1f = 3.14159268;
	emgr.addEntity(e0);

	Entity e1 = emgr.createEntity();
	int& e2i = emgr.createComponent<int>(e1);
	double& e2d = emgr.createComponent<double>(e1);
	std::string& e2s = emgr.createComponent<std::string>(e1);
	emgr.addEntity(e1);
	
	std::cout << emgr.getBits(e0) << " " << emgr.getBits(e1) << "\n";
	
	SampleSystem ss(emgr);
	SampleSystem2 ss2(emgr);
	SampleSystem3 ss3(emgr);
	
	std::cout << "num observers: " << " " << emgr.m_observers.size() << std::endl;
	ss.process(e0);

	e1i = 3;
	e1d = 0.5;
	e1s = "heh";
	e2i = 123;
	e2d = 0.999;
	e2s = "working!";
	
	ss.process(e0);
	
	std::cout << "===============================\n";
	
	ss.process();

	std::cout << "===============================\n";

	ss2.process();
	//artecshpp::core::Aspect aspect;
	//CheckerIteratorFactory cif(emgr, aspect);

	return 0;
}
