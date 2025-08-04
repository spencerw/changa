################
# VMs
################

resource "openstack_compute_instance_v2" "Ubuntu20" {
  name              = "terraform_Ubuntu20_${count.index}"
  flavor_id         = var.flavor_id
  key_pair          = var.public_key
  security_groups   = ["terraform_ssh_ping", "default"]
  count             = var.vm_number

  network {
    name = "terraform_network"
  }

  image_id = var.image_id

  metadata = {
    terraform_controlled = "yes"
  }

  depends_on = [
    openstack_networking_network_v2.terraform_network
  ]
}

################
# Floating IPs
################

# Create floating IPs from public pool
resource "openstack_networking_floatingip_v2" "terraform_floatip_ubuntu20" {
  pool  = "public"
  count = var.vm_number
}

# Assign floating IPs to instances
resource "openstack_compute_floatingip_associate_v2" "terraform_floatubntu20" {
  floating_ip = openstack_networking_floatingip_v2.terraform_floatip_ubuntu20[count.index].address
  instance_id = openstack_compute_instance_v2.Ubuntu20[count.index].id
  count       = var.vm_number
}

output "floating_ip_ubuntu20" {
  value       = openstack_networking_floatingip_v2.terraform_floatip_ubuntu20[*].address
  description = "Public IP for Ubuntu 20"
}

output "private_ips" {
  value       = openstack_compute_instance_v2.Ubuntu20[*].access_ip_v4
  description = "Private IP addresses of Ubuntu20 instances"
}
