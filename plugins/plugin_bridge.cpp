#include <solanaceae/plugin/solana_plugin_v1.h>

#include "../src/bridge.hpp"

#include <solanaceae/util/config_model.hpp>
#include <solanaceae/message3/message_command_dispatcher.hpp>

#include <memory>
#include <limits>
#include <iostream>

static std::unique_ptr<Bridge> g_bridge = nullptr;

constexpr const char* plugin_name = "Bridge";

extern "C" {

SOLANA_PLUGIN_EXPORT const char* solana_plugin_get_name(void) {
	return plugin_name;
}

SOLANA_PLUGIN_EXPORT uint32_t solana_plugin_get_version(void) {
	return SOLANA_PLUGIN_VERSION;
}

SOLANA_PLUGIN_EXPORT uint32_t solana_plugin_start(struct SolanaAPI* solana_api) {
	std::cout << "PLUGIN " << plugin_name << " START()\n";

	if (solana_api == nullptr) {
		return 1;
	}

	try {
		auto* cr = PLUG_RESOLVE_INSTANCE_VERSIONED(Contact3Registry, "1");
		auto* rmm = PLUG_RESOLVE_INSTANCE(RegistryMessageModelI);
		auto* conf = PLUG_RESOLVE_INSTANCE(ConfigModelI);

		// optional dep
		auto* mcd = PLUG_RESOLVE_INSTANCE_OPT(MessageCommandDispatcher);

		// static store, could be anywhere tho
		// construct with fetched dependencies
		g_bridge = std::make_unique<Bridge>(*cr, *rmm, *conf, mcd);

		// register types
		PLUG_PROVIDE_INSTANCE(Bridge, plugin_name, g_bridge.get());
	} catch (const ResolveException& e) {
		std::cerr << "PLUGIN " << plugin_name << " " << e.what << "\n";
		return 2;
	}

	return 0;
}

SOLANA_PLUGIN_EXPORT void solana_plugin_stop(void) {
	std::cout << "PLUGIN " << plugin_name << " STOP()\n";

	g_bridge.reset();
}

SOLANA_PLUGIN_EXPORT float solana_plugin_tick(float time_delta) {
	g_bridge->iterate(time_delta);

	return std::numeric_limits<float>::max();
}

// no render

} // extern C

