#include "./bridge.hpp"

#include <solanaceae/util/config_model.hpp>
#include <solanaceae/util/utils.hpp>
#include <solanaceae/util/time.hpp>
#include <solanaceae/contact/contact_store_i.hpp>
#include <solanaceae/contact/components.hpp>
#include <solanaceae/message3/components.hpp>
#include <solanaceae/message3/message_command_dispatcher.hpp>

#include <iostream>

Bridge::Bridge(
	ContactStore4I& cs,
	RegistryMessageModelI& rmm,
	ConfigModelI& conf,
	MessageCommandDispatcher* mcd
) : _cs(cs), _rmm(rmm), _rmm_sr(_rmm.newSubRef(this)), _conf(conf), _mcd(mcd) {
	_rmm_sr.subscribe(RegistryMessageModel_Event::message_construct);

	if (!_conf.has_bool("Bridge", "username_angle_brackets")) {
		_conf.set("Bridge", "username_angle_brackets", true);
	}
	if (!_conf.has_bool("Bridge", "username_id")) {
		_conf.set("Bridge", "username_id", true);
	}
	if (!_conf.has_bool("Bridge", "username_colon")) {
		_conf.set("Bridge", "username_colon", true);
	}

	// load synced contacts (bridged groups)
	std::map<std::string, size_t> tmp_name_to_id;
	for (const auto [contact_id, vgroup_str] : _conf.entries_string("Bridge", "contact_to_vgroup")) {
		const auto tmp_vgroup_str = std::string{vgroup_str.start, vgroup_str.start+vgroup_str.extend};
		if (!tmp_name_to_id.count(tmp_vgroup_str)) {
			tmp_name_to_id[tmp_vgroup_str] = _vgroups.size();
			_vgroups.emplace_back().vg_name = tmp_vgroup_str;
		}
		auto& v_group = _vgroups.at(tmp_name_to_id.at(tmp_vgroup_str));

		auto& new_vgc = v_group.contacts.emplace_back();
		//new_vgc.c = {}; // TODO: does this work here?
		new_vgc.c = _cs.contactHandle(entt::null);
		new_vgc.id = hex2bin(contact_id);
	}

	updateVGroups();

	registerCommands();
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
				auto view = _cs.registry().view<Contact::Components::TagBig, Contact::Components::ID>();
				for (const auto c : view) {
					if (view.get<Contact::Components::ID>(c).data == vgc.id) {
						vgc.c = _cs.contactHandle(c);
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

void Bridge::registerCommands(void) {
	if (_mcd == nullptr) {
		return;
	}

	_mcd->registerCommand(
		"Bridge", "bridge",
		"users",
		[this](std::string_view params, Message3Handle m) -> bool {
			const auto contact_from = m.get<Message::Components::ContactFrom>().c;
			const auto contact_to = m.get<Message::Components::ContactTo>().c;

			ContactHandle4 group_contact;
			const VirtualGroups* vg = nullptr;

			if (!params.empty()) { // vgroup_name supplied
				for (const auto& vg_it : _vgroups) {
					if (vg_it.vg_name == params) {
						vg = &vg_it;
						break;
					}
				}
				if (vg == nullptr) {
					_rmm.sendText(
						contact_from,
						"The supplied vgroup name does not exist!"
					);
					return true;
				}
			} else {
				if (/*is public ?*/ _c_to_vg.count(_cs.contactHandle(contact_to))) {
					// message was sent public in group
					group_contact = _cs.contactHandle(contact_to);
				} else if (_cs.registry().all_of<Contact::Components::Parent>(contact_from)) {
					// use parent of sender
					group_contact = _cs.contactHandle(_cs.registry().get<Contact::Components::Parent>(contact_from).parent);
				} else if (false /* parent of contact_to ? */) {
				}

				if (!_c_to_vg.count(group_contact)) {
					// nope
					_rmm.sendText(
						contact_from,
						"It appears you are not bridged or forgot to supply the vgroup name!"
					);
					return true;
				}

				assert(static_cast<bool>(group_contact));
				vg = &_vgroups.at(_c_to_vg.at(group_contact));
			}

			assert(vg != nullptr);

			_rmm.sendText(
				contact_from,
				"Contacts online in other bridged group(s) in vgroup '" + vg->vg_name + "'"
			);
			for (const auto& vgc : vg->contacts) {
				if (vgc.c == group_contact) {
					// skip self
					continue;
				}

				if (vgc.c.all_of<Contact::Components::ParentOf>()) {
					_rmm.sendText(
						contact_from,
						"  online in '" +
						(vgc.c.all_of<Contact::Components::Name>() ? vgc.c.get<Contact::Components::Name>().name : "<unk>") +
						"' id:" + bin2hex(vgc.id)
					);

					const auto& cr = _cs.registry();

					// for each sub contact
					for (const auto& sub_c : vgc.c.get<Contact::Components::ParentOf>().subs) {
						if (
							const auto* sub_cs = cr.try_get<Contact::Components::ConnectionState>(sub_c);
							sub_cs != nullptr && sub_cs->state == Contact::Components::ConnectionState::disconnected
						) {
							// skip offline
							continue;
						}

						_rmm.sendText(
							contact_from,
							"    '" +
							(cr.all_of<Contact::Components::Name>(sub_c) ? cr.get<Contact::Components::Name>(sub_c).name : "<unk>") +
							"'" +
							(cr.all_of<Contact::Components::ID>(sub_c) ? " id:" + bin2hex(cr.get<Contact::Components::ID>(sub_c).data) : "")
						);
					}

				} else { // contact without parent
					_rmm.sendText(
						contact_from,
						"  online contact '" +
						(vgc.c.all_of<Contact::Components::Name>() ? vgc.c.get<Contact::Components::Name>().name : "<unk>") +
						"' id:" + bin2hex(vgc.id)
					);
				}
			}
			return true;
		},
		"List users connected in the other bridged group(s).",
		MessageCommandDispatcher::Perms::EVERYONE // TODO: should proably be MODERATOR
	);

	std::cout << "Bridge: registered commands\n";
}

const Bridge::VirtualGroups* Bridge::findVGforContact(const ContactHandle4& c) {
	return nullptr;
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
		int64_t time_diff = int64_t(getTimeMS()) - int64_t(e.e.get<Message::Components::Timestamp>().ts);
		if (time_diff > 5*1000*60) {
			return false; // message too old
		}
	}

	const auto& cr = _cs.registry();

	const auto contact_from = e.e.get<Message::Components::ContactFrom>().c;
	if (cr.any_of<Contact::Components::TagSelfStrong, Contact::Components::TagSelfWeak>(contact_from)) {
		return false; // skip own messages
	}

	const auto contact_to = e.e.get<Message::Components::ContactTo>().c;
	// if e.e <contact to> is in c to vg
	const auto it = _c_to_vg.find(_cs.contactHandle(contact_to));
	if (it == _c_to_vg.cend()) {
		return false; // contact is not bridged
	}
	const auto& vgroup = _vgroups.at(it->second);
	const auto& message_text = e.e.get<Message::Components::MessageText>().text;
	const bool is_action = e.e.all_of<Message::Components::TagMessageIsAction>();

	const bool use_angle_brackets = _conf.get_bool("Bridge", "username_angle_brackets", vgroup.vg_name).value();

	std::string from_str;
	if (use_angle_brackets) {
		from_str += "<";
	}

	if (cr.all_of<Contact::Components::Name>(contact_from)) {
		const auto& name = cr.get<Contact::Components::Name>(contact_from).name;
		if (name.empty()) {
			from_str += "UNK";
		} else {
			from_str += name.substr(0, 16);
		}
	}

	if (_conf.get_bool("Bridge", "username_id", vgroup.vg_name).value()) {
		if (cr.all_of<Contact::Components::ID>(contact_from)) {
			// copy
			auto id = cr.get<Contact::Components::ID>(contact_from).data;
			id.resize(3); // TODO:make size configurable

			// TODO: make seperator configurable
			from_str += "#" + bin2hex(id);
		}
	}

	if (use_angle_brackets) {
		from_str += ">";
	}

	if (_conf.get_bool("Bridge", "username_colon", vgroup.vg_name).value()) {
		from_str += ":";
	}

	from_str += " ";

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

