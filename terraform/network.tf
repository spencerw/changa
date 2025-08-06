################
# Networking
################

data "openstack_networking_network_v2" "spencerw_network_changa_cicd" {
  name = "spencerw_network_changa_cicd"
}

data "openstack_networking_subnet_v2" "spencerw_subnet1_changa_cicd" {
  name = "spencerw_subnet1_changa_cicd"
}

data "openstack_networking_router_v2" "spencerw_router_changa_cicd" {
  name = "spencerw_router_changa_cicd"
}
