variable "vm_number" {
  default = "1"
}

variable "flavor_id" {
  default = "4"
}

variable "public_key" {
  # replace this with the name of the public ssh key you uploaded to Jetstream 2
  # https://docs.jetstream-cloud.org/ui/cli/managing-ssh-keys/
  default = "spencerw-api-key"
}

variable "image_id" {
  # replace this with the image id of the ubuntu iso you want to use
  default = "85f48f90-f1ff-4311-b5ad-d95e8c4c9d23"
}

variable "network_id" {
  # replace this with the id of the public interface on JS
  default = "3fe22c05-6206-4db2-9a13-44f04b6796e6"
}
