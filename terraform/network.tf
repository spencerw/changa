################
# Networking
################
resource "openstack_networking_network_v2" "terraform_network_changa" {
  name           = "terraform_network_changa"
  admin_state_up = true
}

resource "openstack_networking_subnet_v2" "terraform_subnet1_changa" {
  name       = "terraform_subnet1_changa"
  network_id = openstack_networking_network_v2.terraform_network_changa.id
  cidr       = "192.168.1.0/24"
  ip_version = 4
}

resource "openstack_networking_router_v2" "terraform_router_changa" {
  name                = "terraform_router_changa"
  admin_state_up      = true
  external_network_id = var.network_id
}

resource "openstack_networking_router_interface_v2" "terraform_router_interface_1_changa" {
  router_id = openstack_networking_router_v2.terraform_router_changa.id
  subnet_id = openstack_networking_subnet_v2.terraform_subnet1_changa.id
}
