# infra-tekton-labs
Examples of CI/CD used to support Zcash build and testing

# Environment Tooling Setup
### Install Docker
https://docs.docker.com/engine/install/ 

#### Setup Docker 
If you are installing Docker for the first time, to run `docker` without `sudo` priviledge for every docker command:

    - Check if Docker group already exists:
        - `cat /etc/group | grep docker`
        - How to create docker group:
            - `sudo groupadd docker`
    - Add current user to docker group
        - `sudo usermod -a -G docker $CURR_USER`
    - Validate current user was added:
        - `cat /etc/group | grep docker`
    - Restart system to persist changes and graceful setup (had issues with just logout/login)

### Install Kind
https://kind.sigs.k8s.io/

### Install Kubctl
https://kubernetes.io/docs/tasks/tools/install-kubectl-linux/ 

### Install Tkn
https://tekton.dev/docs/cli/

# Setup Local Zcash CI environment

### Create cluster (e.g. micro k8, kind, minicube)
1. `kind create cluster --name zcash-tekton-lab`

    Other Useful commands
    - Validate cluster creation & context: `kubectl cluster-info --context kind-zcash-tekton-lab`

    - Delete cluster: `kind delete cluster --name zcash-tekton-lab || true`

    - List Kube contexts: `kubectl config get-contexts`

    - Delete Kube contexts: `kubectl config delete-context <context_name>`

    - Setup Kube context: `kubectl config current-context`

    - Create new Kube context: `kubectl config set-context zcash_local_ci --user=cluster-admin`

    - Switch to new context: `kubectl config use-context zcash_local_ci`

### Create Tekton Pipeline in cluster
See: https://github.com/tektoncd/pipeline for recent version
2. `kubectl apply -f releases/tekton-pipeline-v0.37.0.yaml`
    
    (Alternative)
   `kubectl apply -f https://storage.googleapis.com/tekton-releases/pipeline/previous/v0.37.0/release.yaml`

### Create Tekton Dashboard in cluster
See: https://github.com/tektoncd/dashboard for recent version
3. `kubectl apply -f releases/tekton-dashboard-readonly-v0.27.0.yaml`

    Validate deployment of Tekton Dashboard & Pipeline
    `kubectl get pods --namespace tekton-pipelines`

    Forward Tekton Dashboard:
    `kubectl --namespace tekton-pipelines port-forward svc/tekton-dashboard 9097:9097 &`

    View Dashboard in Browser:
    `http://localhost:9097/`

## Create Tekton Task
kubectl apply -f ./tasks/zcash-build.yml

tkn task start --showlog zcash-build

tkn task delete zcash-build

## Create Tekton Pipeline
