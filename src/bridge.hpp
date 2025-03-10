#pragma once

#include <solanaceae/contact/fwd.hpp>
#include <solanaceae/message3/registry_message_model.hpp>

#include <vector>
#include <map>
#include <string>
#include <cstdint>

// fwd
struct ConfigModelI;
class MessageCommandDispatcher;

class Bridge : public RegistryMessageModelEventI {
	ContactStore4I& _cs;
	RegistryMessageModelI& _rmm;
	RegistryMessageModelI::SubscriptionReference _rmm_sr;
	ConfigModelI& _conf;
	MessageCommandDispatcher* _mcd;

	struct VirtualGroups {
		struct VContact {
			ContactHandle4 c; // might be null
			std::vector<uint8_t> id; // if contact appears, we check
		};
		std::vector<VContact> contacts;

		std::string vg_name;
		// TODO: cache settings here?
	};
	std::vector<VirtualGroups> _vgroups;
	std::map<ContactHandle4, size_t> _c_to_vg;

	float _iterate_timer {0.f};

	public:
		static constexpr const char* version {"1"};

		Bridge(
			ContactStore4I& cs,
			RegistryMessageModelI& rmm,
			ConfigModelI& conf,
			MessageCommandDispatcher* mcd = nullptr
		);
		~Bridge(void);

		void iterate(float time_delta);

	private:
		void updateVGroups(void);
		void registerCommands(void);
		const VirtualGroups* findVGforContact(const ContactHandle4& c);

	protected: // mm
		bool onEvent(const Message::Events::MessageConstruct& e) override;
};

