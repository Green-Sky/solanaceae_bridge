#pragma once

#include <cstdint>
#include <solanaceae/message3/registry_message_model.hpp>
#include <solanaceae/contact/contact_model3.hpp>

#include <vector>
#include <map>

// fwd
struct ConfigModelI;

class Bridge : public RegistryMessageModelEventI {
	Contact3Registry& _cr;
	RegistryMessageModel& _rmm;
	ConfigModelI& _conf;

	struct VirtualGroups {
		struct VContact {
			Contact3Handle c; // might be null
			std::vector<uint8_t> id; // if contact appears, we check
		};
		std::vector<VContact> contacts;

		// metadata/settings?
	};
	std::vector<VirtualGroups> _vgroups;
	std::map<Contact3Handle, size_t> _c_to_vg;

	float _iterate_timer {0.f};

	public:
		Bridge(
			Contact3Registry& cr,
			RegistryMessageModel& rmm,
			ConfigModelI& conf
		);
		~Bridge(void);

		void iterate(float time_delta);

	private:
		void updateVGroups(void);

	protected: // mm
		bool onEvent(const Message::Events::MessageConstruct& e) override;
};

