// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include <string.h>

#include <vector>

#include "common/ceph_json.h"
#include "rgw_common.h"
#include "rgw_user.h"
#include "rgw_acl_swift.h"

#define dout_subsys ceph_subsys_rgw

using namespace std;

#define SWIFT_PERM_READ  RGW_PERM_READ_OBJS
#define SWIFT_PERM_WRITE RGW_PERM_WRITE_OBJS
/* FIXME: do we really need separate RW? */
#define SWIFT_PERM_RWRT  (SWIFT_PERM_READ | SWIFT_PERM_WRITE)
#define SWIFT_PERM_ADMIN RGW_PERM_FULL_CONTROL

#define SWIFT_GROUP_ALL_USERS ".r:*"

static int parse_list(const std::string& uid_list,
                      std::vector<std::string>& uids)           /* out */
{
  char *s = strdup(uid_list.c_str());
  if (!s) {
    return -ENOMEM;
  }

  char *tokctx;
  const char *p = strtok_r(s, " ,", &tokctx);
  while (p) {
    if (*p) {
      string acl = p;
      uids.push_back(acl);
    }
    p = strtok_r(NULL, " ,", &tokctx);
  }
  free(s);
  return 0;
}

static bool uid_is_public(const string& uid)
{
  if (uid[0] != '.' || uid[1] != 'r')
    return false;

  int pos = uid.find(':');
  if (pos < 0 || pos == (int)uid.size())
    return false;

  string sub = uid.substr(0, pos);
  string after = uid.substr(pos + 1);

  if (after.compare("*") != 0)
    return false;

  return sub.compare(".r") == 0 ||
         sub.compare(".referer") == 0 ||
         sub.compare(".referrer") == 0;
}

static bool extract_referer_urlspec(const std::string& uid,
                                    std::string& url_spec)
{
  const size_t pos = uid.find(':');
  if (string::npos == pos) {
    return false;
  }

  const auto sub = uid.substr(0, pos);
  url_spec = uid.substr(pos + 1);

  return sub.compare(".r") == 0 ||
         sub.compare(".referer") == 0 ||
         sub.compare(".referrer") == 0;
}

static bool normalize_referer_urlspec(string& url_spec, bool& is_negative)
{
  try {
    if ('-' == url_spec[0]) {
      is_negative = true;
      url_spec = url_spec.substr(1);
    } else {
      is_negative = false;
    }
    if (url_spec != "*" && '*' == url_spec[0]) {
      url_spec = url_spec.substr(1);
    }

    return !url_spec.empty() && url_spec != ".";
  } catch (std::out_of_range) {
    return false;
  }
}

void RGWAccessControlPolicy_SWIFT::add_grants(RGWRados * const store,
                                              const std::vector<std::string>& uids,
                                              const uint32_t perm)
{
  for (const auto& uid : uids) {
    ldout(cct, 20) << "trying to add grant for ACL uid=" << uid << dendl;
    ACLGrant grant;
    string url_spec;

    if (uid_is_public(uid)) {
      grant.set_group(ACL_GROUP_ALL_USERS, perm);
      acl.add_grant(&grant);
    } else if (extract_referer_urlspec(uid, url_spec)) {
      if (0 != (perm & SWIFT_PERM_WRITE)) {
        ldout(cct, 10) << "cannot grant write access for referers" << dendl;
        continue;
      }

      bool is_negative = false;
      if (false == normalize_referer_urlspec(url_spec, is_negative)) {
        ldout(cct, 10) << "cannot normalize referer: " << url_spec << dendl;
        continue;
      } else {
        ldout(cct, 10) << "normalized referer to url_spec=" << url_spec
                       << ", is_negative=" << is_negative << dendl;
      }

      if (is_negative) {
        /* Forbid access. */
        grant.set_referer(url_spec, 0);
      } else {
        grant.set_referer(url_spec, perm);
      }

      acl.add_grant(&grant);
    } else {
      rgw_user user(uid);
      RGWUserInfo grant_user;

      if (rgw_get_user_info_by_uid(store, user, grant_user) < 0) {
        ldout(cct, 10) << "grant user does not exist: " << uid << dendl;
        /* skipping silently */
        grant.set_canon(user, std::string(), perm);
        acl.add_grant(&grant);
      } else {
        grant.set_canon(user, grant_user.display_name, perm);
        acl.add_grant(&grant);
      }
    }
  }
}

bool RGWAccessControlPolicy_SWIFT::create(RGWRados * const store,
                                          const rgw_user& id,
                                          const std::string& name,
                                          const std::string& read_list,
                                          const std::string& write_list)
{
  acl.create_default(id, name);
  owner.set_id(id);
  owner.set_name(name);

  if (read_list.size()) {
    std::vector<std::string> uids;
    int r = parse_list(read_list, uids);
    if (r < 0) {
      ldout(cct, 0) << "ERROR: parse_list returned r=" << r << dendl;
      return false;
    }

    add_grants(store, uids, SWIFT_PERM_READ);
  }
  if (write_list.size()) {
    std::vector<std::string> uids;
    int r = parse_list(write_list, uids);
    if (r < 0) {
      ldout(cct, 0) << "ERROR: parse_list returned r=" << r << dendl;
      return false;
    }

    add_grants(store, uids, SWIFT_PERM_WRITE);
  }
  return true;
}

void RGWAccessControlPolicy_SWIFT::to_str(string& read, string& write)
{
  multimap<string, ACLGrant>& m = acl.get_grant_map();
  multimap<string, ACLGrant>::iterator iter;

  for (iter = m.begin(); iter != m.end(); ++iter) {
    ACLGrant& grant = iter->second;
    const uint32_t perm = grant.get_permission().get_permissions();
    rgw_user id;
    if (!grant.get_id(id)) {
      if (grant.get_group() != ACL_GROUP_ALL_USERS)
        continue;
      id = SWIFT_GROUP_ALL_USERS;
    }
    if (perm & SWIFT_PERM_READ) {
      if (!read.empty()) {
        read.append(",");
      }
      read.append(id.to_str());
    } else if (perm & SWIFT_PERM_WRITE) {
      if (!write.empty()) {
        write.append(",");
      }
      write.append(id.to_str());
    }
  }
}

void RGWAccessControlPolicy_SWIFTAcct::add_grants(RGWRados * const store,
                                                  const std::vector<std::string>& uids,
                                                  const uint32_t perm)
{
  for (const auto& uid : uids) {
    ACLGrant grant;
    RGWUserInfo grant_user;

    if (uid_is_public(uid)) {
      grant.set_group(ACL_GROUP_ALL_USERS, perm);
      acl.add_grant(&grant);
    } else  {
      rgw_user user(uid);

      if (rgw_get_user_info_by_uid(store, user, grant_user) < 0) {
        ldout(cct, 10) << "grant user does not exist:" << uid << dendl;
        /* skipping silently */
        grant.set_canon(user, std::string(), perm);
        acl.add_grant(&grant);
      } else {
        grant.set_canon(user, grant_user.display_name, perm);
        acl.add_grant(&grant);
      }
    }
  }
}

bool RGWAccessControlPolicy_SWIFTAcct::create(RGWRados * const store,
                                              const rgw_user& id,
                                              const std::string& name,
                                              const std::string& acl_str)
{
  acl.create_default(id, name);
  owner.set_id(id);
  owner.set_name(name);

  JSONParser parser;

  if (!parser.parse(acl_str.c_str(), acl_str.length())) {
    ldout(cct, 0) << "ERROR: JSONParser::parse returned error=" << dendl;
    return false;
  }

  JSONObjIter iter = parser.find_first("admin");
  if (!iter.end() && (*iter)->is_array()) {
    std::vector<std::string> admin;
    decode_json_obj(admin, *iter);
    ldout(cct, 0) << "admins: " << admin << dendl;

    add_grants(store, admin, SWIFT_PERM_ADMIN);
  }

  iter = parser.find_first("read-write");
  if (!iter.end() && (*iter)->is_array()) {
    std::vector<std::string> readwrite;
    decode_json_obj(readwrite, *iter);
    ldout(cct, 0) << "read-write: " << readwrite << dendl;

    add_grants(store, readwrite, SWIFT_PERM_RWRT);
  }

  iter = parser.find_first("read-only");
  if (!iter.end() && (*iter)->is_array()) {
    std::vector<std::string> readonly;
    decode_json_obj(readonly, *iter);
    ldout(cct, 0) << "read-only: " << readonly << dendl;

    add_grants(store, readonly, SWIFT_PERM_READ);
  }

  return true;
}

void RGWAccessControlPolicy_SWIFTAcct::to_str(std::string& acl_str) const
{
  std::vector<std::string> admin;
  std::vector<std::string> readwrite;
  std::vector<std::string> readonly;

  /* Parition the grant map into three not-overlapping groups. */
  for (const auto& item : get_acl().get_grant_map()) {
    const ACLGrant& grant = item.second;
    const uint32_t perm = grant.get_permission().get_permissions();

    rgw_user id;
    if (!grant.get_id(id)) {
      if (grant.get_group() != ACL_GROUP_ALL_USERS) {
        continue;
      }
      id = SWIFT_GROUP_ALL_USERS;
    } else if (owner.get_id() == id) {
      continue;
    }

    if (SWIFT_PERM_ADMIN == (perm & SWIFT_PERM_ADMIN)) {
      admin.insert(admin.end(), id.to_str());
    } else if (SWIFT_PERM_RWRT == (perm & SWIFT_PERM_RWRT)) {
      readwrite.insert(readwrite.end(), id.to_str());
    } else if (SWIFT_PERM_READ == (perm & SWIFT_PERM_READ)) {
      readonly.insert(readonly.end(), id.to_str());
    } else {
      // FIXME: print a warning
    }
  }

  /* Serialize the groups. */
  JSONFormatter formatter;

  formatter.open_object_section("acl");
  if (!readonly.empty()) {
    encode_json("read-only", readonly, &formatter);
  }
  if (!readwrite.empty()) {
    encode_json("read-write", readwrite, &formatter);
  }
  if (!admin.empty()) {
    encode_json("admin", admin, &formatter);
  }
  formatter.close_section();

  std::ostringstream oss;
  formatter.flush(oss);

  acl_str = oss.str();
}
