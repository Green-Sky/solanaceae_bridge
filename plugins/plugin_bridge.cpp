#include <solanaceae/plugin/solana_plugin_v1.h>

#include "../src/bridge.hpp"

#include <memory>
#include <iostream>

#define RESOLVE_INSTANCE(x) static_cast<x*>(solana_api->resolveInstance(#x))
#define PROVIDE_INSTANCE(x, p, v) solana_api->provideInstance(#x, p, static_cast<x*>(v))

static std::unique_ptr<Bridge> g_bridge = nullptr;

extern "C" {

SOLANA_PLUGIN_EXPORT const char* solana_plugin_get_name(void) {
	return "Bridge";
}

SOLANA_PLUGIN_EXPORT uint32_t solana_plugin_get_version(void) {
	return SOLANA_PLUGIN_VERSION;
}

SOLANA_PLUGIN_EXPORT uint32_t solana_plugin_start(struct SolanaAPI* solana_api) {
	std::cout << "PLUGIN Bridge START()\n";

	if (solana_api == nullptr) {
		return 1;
	}

	Contact3Registry* cr;
	RegistryMessageModel* rmm = nullptr;
	ConfigModelI* conf = nullptr;

	{ // make sure required types are loaded
		cr = RESOLVE_INSTANCE(Contact3Registry);
		rmm = RESOLVE_INSTANCE(RegistryMessageModel);
		conf = RESOLVE_INSTANCE(ConfigModelI);

		if (cr == nullptr) {
			std::cerr << "PLUGIN Bridge missing Contact3Registry\n";
			return 2;
		}

		if (rmm == nullptr) {
			std::cerr << "PLUGIN Bridge missing RegistryMessageModel\n";
			return 2;
		}

		if (conf == nullptr) {
			std::cerr << "PLUGIN Bridge missing ConfigModelI\n";
			return 2;
		}
	}

	// static store, could be anywhere tho
	// construct with fetched dependencies
	g_bridge = std::make_unique<Bridge>(*cr, *rmm, *conf);

	// register types
	PROVIDE_INSTANCE(Bridge, "Bridge", g_bridge.get());

	return 0;
}

SOLANA_PLUGIN_EXPORT void solana_plugin_stop(void) {
	std::cout << "PLUGIN Bridge STOP()\n";

	g_bridge.reset();
}

SOLANA_PLUGIN_EXPORT void solana_plugin_tick(float delta) {
	(void)delta;
	//std::cout << "PLUGIN Bridge TICK()\n";
	g_bridge->iterate(delta);
}

} // extern C

