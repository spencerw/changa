################
# VMs
################

resource "openstack_compute_instance_v2" "Ubuntu20" {
  name              = "terraform_Ubuntu20_${count.index}"
  flavor_id         = var.flavor_id
  key_pair          = var.public_key
  security_groups = ["spencerw_ssh_ping_changa_cicd", "default"]
  count             = var.vm_number

  network {
    uuid = data.openstack_networking_network_v2.spencerw_network_changa_cicd.id
  }

  image_id = var.image_id

  metadata = {
    terraform_controlled = "yes"
  }

  user_data = templatefile("${path.module}/cloud_init.yaml.tmpl", {
    jetstream_public_key = file("${path.module}/jetstream_key.pub")
    mpi_public_key = file("${path.module}/mpi_key.pub")
  })
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

output "public_ips" {
  value       = openstack_networking_floatingip_v2.terraform_floatip_ubuntu20[*].address
  description = "Public IP addresses of Ubuntu 20 instances"
}

output "private_ips" {
  value       = openstack_compute_instance_v2.Ubuntu20[*].access_ip_v4
  description = "Private IP addresses of Ubuntu20 instances"
}

output "rendered_user_data" {
  value     = templatefile("${path.module}/cloud_init.yaml.tmpl", {
    jetstream_public_key  = file("${path.module}/jetstream_key.pub")
    mpi_public_key        = file("${path.module}/mpi_key.pub")
  })
}
