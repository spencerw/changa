################
#Security section
################
#creating security group
resource "openstack_compute_secgroup_v2" "terraform_ssh_ping_centos" {
  name        = "terraform_ssh_ping"
  description = "Security group with SSH and PING open to private subnet and optionally public"

  # Allow SSH from anywhere (optional, remove if not needed)
  rule {
    ip_protocol = "tcp"
    from_port   = "22"
    to_port     = "22"
    cidr        = "0.0.0.0/0"
  }

  # Allow SSH from private subnet (for MPI node communication)
  rule {
    ip_protocol = "tcp"
    from_port   = "22"
    to_port     = "22"
    cidr        = "192.168.0.0/24"
  }

  # Allow ICMP (ping) from private subnet only
  rule {
    ip_protocol = "icmp"
    from_port   = -1
    to_port     = -1
    cidr        = "192.168.0.0/24"
  }
}
