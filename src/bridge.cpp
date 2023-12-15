#include "./bridge.hpp"

#include <solanaceae/util/config_model.hpp>
#include <solanaceae/contact/components.hpp>
#include <solanaceae/message3/components.hpp>

#include <iostream>

namespace detail {
	constexpr uint8_t nib_from_hex(char c) {
		assert((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));

		if (c >= '0' && c <= '9') {
			return static_cast<uint8_t>(c) - '0';
		} else if (c >= 'a' && c <= 'f') {
			return (static_cast<uint8_t>(c) - 'a') + 10u;
		} else {
			return 0u;
		}
	}

	constexpr char nib_to_hex(uint8_t c) {
		assert(c <= 0x0f);

		if (c <= 0x09) {
			return c + '0';
		} else {
			return (c - 10u) + 'a';
		}
	}
} // detail

static std::vector<uint8_t> hex2bin(const std::string_view str) {
	assert(str.size() % 2 == 0);
	std::vector<uint8_t> bin{};
	bin.resize(str.size()/2, 0);

	for (size_t i = 0; i < bin.size(); i++) {
		bin[i] = detail::nib_from_hex(str[i*2]) << 4 | detail::nib_from_hex(str[i*2+1]);
	}

	return bin;
}

static std::string bin2hex(const std::vector<uint8_t> data) {
	std::string str;
	for (size_t i = 0; i < data.size(); i++) {
		str.push_back(detail::nib_to_hex(data[i] >> 4));
		str.push_back(detail::nib_to_hex(data[i] & 0x0f));
	}
	return str;
}

Bridge::Bridge(
	Contact3Registry& cr,
	RegistryMessageModel& rmm,
	ConfigModelI& conf
) : _cr(cr), _rmm(rmm), _conf(conf) {
	_rmm.subscribe(this, enumType::message_construct);

	// load synced contacts (bridged groups)
	std::map<std::string, size_t> tmp_name_to_id;
	for (const auto [contact_id, vgroup_str] : _conf.entries_string("Bridge", "contact_to_vgroup")) {
		const auto tmp_vgroup_str = std::string{vgroup_str.start, vgroup_str.start+vgroup_str.extend};
		if (!tmp_name_to_id.count(tmp_vgroup_str)) {
			tmp_name_to_id[tmp_vgroup_str] = _vgroups.size();
			_vgroups.emplace_back();;
		}
		auto& v_group = _vgroups.at(tmp_name_to_id.at(tmp_vgroup_str));

		auto& new_vgc = v_group.contacts.emplace_back();
		new_vgc.c = {_cr, entt::null}; // this is annoying af
		new_vgc.id = hex2bin(contact_id);
	}

	updateVGroups();
}

Bridge::~Bridge(void) {
}

void Bridge::iterate(float time_delta) {
	_iterate_timer += time_delta;
	if (_iterate_timer >= 10.f) {
		_iterate_timer = 0.f;

		updateVGroups();
	}
}

void Bridge::updateVGroups(void) {
	// fill in contacts, some contacts might be created late
	for (size_t i_vg = 0; i_vg < _vgroups.size(); i_vg++) {
		for (auto& vgc : _vgroups[i_vg].contacts) {
			assert(!vgc.id.empty());

			if (!vgc.c.valid()) {
				// search
				auto view = _cr.view<Contact::Components::TagBig, Contact::Components::ID>();
				for (const auto c : view) {
					if (view.get<Contact::Components::ID>(c).data == vgc.id) {
						vgc.c = {_cr, c};
						std::cout << "Bridge: found contact for vgroup\n";
						break;
					}
				}
			}

			if (vgc.c.valid()) {
				_c_to_vg[vgc.c] = i_vg;
			}
		}
	}
}

bool Bridge::onEvent(const Message::Events::MessageConstruct& e) {
	if (!e.e.valid()) {
		return false; // how
	}

	if (!e.e.all_of<Message::Components::MessageText>()) {
		return false; // non text message, skip
	}

	if (!e.e.all_of<Message::Components::ContactFrom, Message::Components::ContactTo>()) {
		return false; // how
	}

	if (e.e.all_of<Message::Components::Timestamp>()) {
		int64_t time_diff = int64_t(Message::getTimeMS()) - int64_t(e.e.get<Message::Components::Timestamp>().ts);
		if (time_diff > 5*1000*60) {
			return false; // message too old
		}
	}

	const auto contact_from = e.e.get<Message::Components::ContactFrom>().c;
	if (_cr.any_of<Contact::Components::TagSelfStrong, Contact::Components::TagSelfWeak>(contact_from)) {
		return false; // skip own messages
	}

	const auto contact_to = e.e.get<Message::Components::ContactTo>().c;
	// if e.e <contact to> is in c to vg
	const auto it = _c_to_vg.find(Contact3Handle{_cr, contact_to});
	if (it == _c_to_vg.cend()) {
		return false; // contact is not bridged
	}
	const auto& vgroup = _vgroups.at(it->second);
	const auto& message_text = e.e.get<Message::Components::MessageText>().text;
	const bool is_action = e.e.all_of<Message::Components::TagMessageIsAction>();

	std::string from_str;
	if (_cr.all_of<Contact::Components::Name>(contact_from)) {
		const auto& name = _cr.get<Contact::Components::Name>(contact_from).name;
		if (name.empty()) {
			from_str += "<UNK";
		} else {
			from_str += "<";
			from_str += name.substr(0, 16);
		}
	}
	if (_cr.all_of<Contact::Components::ID>(contact_from)) {
		// copy
		auto id = _cr.get<Contact::Components::ID>(contact_from).data;
		id.resize(3);

		from_str += "#" + bin2hex(id);
	}

	from_str += "> ";

	// for each c in vg not c...
	for (const auto& other_vc : vgroup.contacts) {
		if (other_vc.c == contact_to) {
			continue; // skip self
		}

		// TODO: support fake/virtual contacts. true bridging
		std::string relayed_message {from_str};

		relayed_message += message_text;

		_rmm.sendText(
			other_vc.c,
			relayed_message,
			is_action
		);
	}

	return false;
}

