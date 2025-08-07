#!/bin/bash

# Set up git
export GIT_SSH_COMMAND="ssh -i /changa-network/cicd/changa_dashboard_deploy_key -o StrictHostKeyChecking=no"

# Clone the dashboard repo
rm -rf changa_dashboard
git clone git@github.com:spencerw/changa_dashboard.git
cd changa_dashboard

# Copy the plot file
cp ../plot.png .

# Commit and push
git add plot.png
git commit -m "Update plot"
git push
